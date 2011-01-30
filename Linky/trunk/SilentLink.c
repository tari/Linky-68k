#include "SilentLink.h"

const unsigned char deviceDescriptor[] = {0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x51, 0x04, 0x04, 0xE0, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01};
const unsigned char configDescriptor[] = {0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0xE0, 0x00, 0x09, 0x04, 0x00, 0x00, 0x02, 0xFF, 0x01, 0x00, 0x00,
                           0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00, 0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00};

USBPeripheral periph;
INT_HANDLER saved_int_1;
INT_HANDLER saved_int_5;
unsigned int SilentLink_RawPacketMaxSize;
void* SilentLink_ReceivedVirtualPacket;
unsigned int SilentLink_CurrentPacketOffset;
unsigned int SilentLink_CurrentPacketSize;
unsigned short SilentLink_CurrentPacketCommandID;

USBPeripheral SilentLink_GetInterface()
{
  USBPeripheral ret = DEFAULT_USB_PERIPHERAL;

  //Set descriptor information
  ret.deviceDescriptor = deviceDescriptor;
  ret.configDescriptor = configDescriptor;

  return ret;
}

void SilentLink_Initialize()
{
	//Set default values
	SilentLink_ReceivedVirtualPacket = NULL;
	SilentLink_RawPacketMaxSize = 0x3FF;
	
	periph = SilentLink_GetInterface();
	Driver_SetPeripheralInterface(&periph);

	//Save AUTO_INT_1 and AUTO_INT_5 interrupts because they interfere with key reading
	saved_int_1 = GetIntVec(AUTO_INT_1);
	saved_int_5 = GetIntVec(AUTO_INT_5);
	SetIntVec(AUTO_INT_1, DUMMY_HANDLER);
	SetIntVec(AUTO_INT_5, DUMMY_HANDLER);

	//Restart the controller
	USB_PeripheralKill();
	USB_PeripheralInitialize();
}

void SilentLink_Kill()
{
	//Cut power to the port
	USB_PeripheralKill();

	//Restore AUTO_INT_1 and AUTO_INT_5 interrupts
  SetIntVec(AUTO_INT_1, saved_int_1);
  SetIntVec(AUTO_INT_5, saved_int_5);
}

unsigned int SilentLink_ReceiveData(unsigned char* buffer, unsigned int count)
{
	unsigned char* dataBuffer = buffer;
	unsigned int bytesToReceive;
	unsigned int bytesReceived = 0;

	while (count > 0)
	{
		if (USB_IsDataReady(0x02))
		{
			bytesToReceive = count > 0x40? 0x40 : count;
			bytesReceived += USB_ReceiveBulkData(0x02, dataBuffer, bytesToReceive);
	
			dataBuffer += bytesReceived;
			count -= bytesReceived;
		}
	}
	
	return bytesReceived;
}

void SilentLink_SendAcknowledgement()
{
	unsigned char buffer[7] = {0};
	
	//Prepare the packet
	buffer[3] = 0x02;
	buffer[4] = 0x05;
	buffer[5] = 0xE0;
	
	USB_SendBulkData(0x01, buffer, 7);
}

void SilentLink_AddRawPacket(unsigned char* buffer, unsigned int count)
{
	//Assume we'll be copying the entire packet's data
	unsigned char* copyBuffer = buffer;
	unsigned char copySize = count;

	//Do we already have memory allocated for the virtual packet?
	if (SilentLink_ReceivedVirtualPacket == NULL)
	{
		//No, so this is the first/only raw packet for this virtual packet
		//Get its information
		SilentLink_CurrentPacketCommandID = (buffer[4] << 8) | buffer[5];
		SilentLink_CurrentPacketSize = (buffer[2] << 8) | buffer[3];
		SilentLink_CurrentPacketOffset = 0;
		
		//Allocate the memory for it
		SilentLink_ReceivedVirtualPacket = malloc(SilentLink_CurrentPacketSize);
		
		//We won't be including the size and command bytes from this raw packet
		copySize -= 6;
		copyBuffer += 6;
	}

	//Now copy the raw packet's data to it
	memcpy(SilentLink_ReceivedVirtualPacket+SilentLink_CurrentPacketOffset, copyBuffer, copySize);
	SilentLink_CurrentPacketOffset += copySize;
}

void SilentLink_SendVirtualPacket(short commandID, unsigned char* buffer, unsigned int count)
{
	//First calculate how many packets we need to send this
	unsigned int rawPackets = 0;
	unsigned int remaining = count + 6;
	do
	{
		remaining -= (SilentLink_RawPacketMaxSize > remaining)? remaining : SilentLink_RawPacketMaxSize;
		if (remaining > 0) rawPackets++;
	}	while (remaining > 0);
	remaining = count + 6;

	//Now send all the type 0x03 packets
	unsigned int i, j, k;
	unsigned char rawBuffer[SilentLink_RawPacketMaxSize+5];
	unsigned int offset = 11;
	unsigned int bufferOffset = 0;

	//Set up the start of the packet
	rawBuffer[0] = 0x00;
	rawBuffer[1] = 0x00;
	rawBuffer[2] = (SilentLink_RawPacketMaxSize) >> 8;
	rawBuffer[3] = (SilentLink_RawPacketMaxSize) & 0xFF;
	rawBuffer[4] = 0x03;
	rawBuffer[5] = 0x00;
	rawBuffer[6] = 0x00;
	rawBuffer[7] = count >> 8;
	rawBuffer[8] = count & 0xFF;
	rawBuffer[9] = commandID >> 8;
	rawBuffer[10] = commandID & 0xFF;
	if (rawPackets > 0)
	{
		//For each packet...
		for (i = 0; i < (rawPackets - 1); i++)
		{
			//Buffer the data
			k = 0;
			for (j = offset; j < SilentLink_RawPacketMaxSize-(offset-5); j++)
			{
				rawBuffer[j] = buffer[bufferOffset+k];
				k++;
			}
			
			//Send it off
			USB_SendBulkData(0x01, rawBuffer, SilentLink_RawPacketMaxSize+5);
	
			//Get ready for the next raw packet
			bufferOffset += (SilentLink_RawPacketMaxSize-(offset-5));
			offset = 5;
			remaining -= (SilentLink_RawPacketMaxSize-(offset-5));
		}
	}

	//Now send the final 0x04 packet
	//Set up the start of the packet
	rawBuffer[0] = 0x00;
	rawBuffer[1] = 0x00;
	rawBuffer[2] = remaining >> 8;
	rawBuffer[3] = remaining & 0xFF;
	rawBuffer[4] = 0x04;

	//Buffer the data
	k = 0;
	for (j = offset; j < SilentLink_RawPacketMaxSize-(offset-5); j++)
	{
		rawBuffer[j] = buffer[bufferOffset+k];
		k++;
	}
	
	//Send it off
	USB_SendBulkData(0x01, rawBuffer, 5);
	USB_SendBulkData(0x01, rawBuffer+5, remaining);

	//Now receive acknowledgement
	unsigned char ack[7];
	SilentLink_ReceiveData(ack, 7);
}

//This is the function you want to modify to add support for new commands/requests.
//You have access to the following:
//	unsigned short SilentLink_CurrentPacketCommandID
//	unsigned int   SilentLink_CurrentPacketSize
//	void*          SilentLink_ReceivedVirtualPacket
//Handle the command in the switch statement below by just acting on it,
//  responding with virtual packets (with SilentLink_SendVirtualPacket),
//  or whatever you want. It has already been acknowledged elsewhere.
void SilentLink_HandleVirtualPacket()
{
	/*printf("Virtual packet received:\n");
	printf(" Command ID: %04X\n", SilentLink_CurrentPacketCommandID);
	printf(" Size: %04X\n", SilentLink_CurrentPacketSize);
	unsigned char buffer[5];
	memcpy(buffer, SilentLink_ReceivedVirtualPacket, 5);
	printf(" Start of data: %02X%02X%02X%02X%02X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);*/

	switch (SilentLink_CurrentPacketCommandID)
	{
		case 0x0001: //Ping / Set Mode
		{
			//Send acknowledgement
			SilentLink_SendVirtualPacket(0x0012, SilentLink_ReceivedVirtualPacket+6, 4);
			break;
		}
		default:
		{
			break;
		}
	}
}

void SilentLink_Do()
{
	if (USB_IsDataReady(0x02))
	{
		unsigned char rawBuffer[SilentLink_RawPacketMaxSize];
		unsigned char buffer[5];
		unsigned int received = SilentLink_ReceiveData(buffer, 0x05);

		if (received == 5)
		{
			//We ignore the first two bytes because we'll never have a raw packet greater than 64KB
			unsigned int rawPacketLength = buffer[3] | (buffer[2] << 8); //| (buffer[1] << 16) | (buffer[0] << 24);
			unsigned int rawPacketType = buffer[4];

			//printf("Received packet, length %04X, type %02X\n", rawPacketLength, rawPacketType);

			switch (rawPacketType)
			{
				case 0x01:
				{
					//Buffer size negotiation, retrieve size it wants to set
					unsigned char bufferSize[4];
					SilentLink_ReceiveData(bufferSize, 4);
	
					//Return buffer negotiaton response
					unsigned char responseBuffer[9] = {0};
					responseBuffer[3] = 0x04;
					responseBuffer[4] = 0x02;
					responseBuffer[7] = SilentLink_RawPacketMaxSize >> 8;
					responseBuffer[8] = SilentLink_RawPacketMaxSize & 0xFF;
					USB_SendBulkData(0x01, responseBuffer, 9);
	
					break;
				}
				case 0x02:
				{
					//Uh...just move on...
					break;
				}
				case 0x03:
				{
					//Receive the rest of the packet somewhere
					SilentLink_ReceiveData(rawBuffer, rawPacketLength);
					
					//Send acknowlegdement
					SilentLink_SendAcknowledgement();

					SilentLink_AddRawPacket(rawBuffer, rawPacketLength);

					break;
				}
				case 0x04:
				{
					//Receive the rest of the packet somewhere
					SilentLink_ReceiveData(rawBuffer, rawPacketLength);
					
					//Send acknowledgement
					SilentLink_SendAcknowledgement();
					
					SilentLink_AddRawPacket(rawBuffer, rawPacketLength);
					
					//Handle this virtual packet
					SilentLink_HandleVirtualPacket();
					
					//Clear this virtual packet from memory now that we're done with it
					free(SilentLink_ReceivedVirtualPacket);
					SilentLink_ReceivedVirtualPacket = NULL;
					
					break;
				}
				case 0x05:
				{
					//This is an acknowledgement
					unsigned char ack[2];
					SilentLink_ReceiveData(ack, 2);
					
					break;
				}
				default:
				{
					//Uh?
					break;
				}
			}
		}
	}
}
