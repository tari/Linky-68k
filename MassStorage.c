#include "MassStorage.h"

const unsigned char deviceDescriptor[] = {0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x81, 0x07, 0x50, 0x51, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01};
const unsigned char configDescriptor[] = {0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0xE0, 0x00, 0x09, 0x04, 0x00, 0x00, 0x02, 0x08, 0x06, 0x50, 0x00,
                           0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00, 0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00};
const unsigned char maxLUN[] = {0x00};

USBPeripheral periph;
INT_HANDLER saved_int_1;
INT_HANDLER saved_int_5;
Handle_ReadSectorRequest  h_readSector;
Handle_WriteSectorRequest h_writeSector;

int MassStorage_UnknownControlRequest(unsigned char bmRequestType, unsigned char bRequest, unsigned int wValue, unsigned int wIndex, unsigned int wLength)
{
	int handled = 0;

	switch(bmRequestType)
	{
		case 0xA1:
		{
			switch(bRequest)
			{
				case 0xFE:
				{
					USB_StartControlOutput(maxLUN, 1);
					handled = 1;
					break;
				}
				default:
					break;
			}
			break;
		}
		default:
			break;
	}

	return handled;
}

USBPeripheral MassStorage_GetInterface(void)
{
  USBPeripheral ret = DEFAULT_USB_PERIPHERAL;

  //Set descriptor information
  ret.deviceDescriptor = deviceDescriptor;
  ret.configDescriptor = configDescriptor;

	//Set callbacks
  ret.h_unknownControlRequest = MassStorage_UnknownControlRequest;

  return ret;
}

void MassStorage_Initialize(Handle_ReadSectorRequest readSector, Handle_WriteSectorRequest writeSector)
{
	periph = MassStorage_GetInterface();
	Driver_SetPeripheralInterface(&periph);

	//Save AUTO_INT_1 and AUTO_INT_5 interrupts because they interfere with key reading
	saved_int_1 = GetIntVec(AUTO_INT_1);
	saved_int_5 = GetIntVec(AUTO_INT_5);
	SetIntVec(AUTO_INT_1, DUMMY_HANDLER);
	SetIntVec(AUTO_INT_5, DUMMY_HANDLER);

	//Restart the controller
	USB_PeripheralKill();
	USB_PeripheralInitialize();
	
	//Set read/write sector handlers
	h_readSector = readSector;
	h_writeSector = writeSector;
}

void MassStorage_Kill(void)
{
	//Cut power to the port
	USB_PeripheralKill();

	//Restore AUTO_INT_1 and AUTO_INT_5 interrupts
  SetIntVec(AUTO_INT_1, saved_int_1);
  SetIntVec(AUTO_INT_5, saved_int_5);
}

unsigned int MassStorage_ReceiveData(unsigned char* buffer, unsigned int count)
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

void MassStorage_Do(void)
{
	if (USB_IsDataReady(0x02))
	{
		unsigned char buffer[31];
		MassStorage_ReceiveData(buffer, 31);

		unsigned int dataTransferLength = (buffer[8] & 0xFF) | (buffer[9] << 8);
		unsigned char dataExpectedBack = buffer[12] & 0x80;
		unsigned char command = buffer[15];

		//printf("Received: L: %04X, D: %02X, C: %02X\n", dataTransferLength, dataExpectedBack, command);

		switch (command)
		{
			case 0x12: //inquiry
			{
				//Send back what it wants
				unsigned char response[36] = {0};
				
				response[1] = 0x80; //removable media
				response[3] = 0x01; //because the UFI spec says so
				response[4] = 0x1F; //additioanl length
				memcpy(response+8, "BrandonW", 8);
				memcpy(response+16, "USB Flash Drive", 15);
				memcpy(response+32, "0.01", 4);
				
				//Send it off
				USB_SendBulkData(0x01, response, 36);
				break;
			}
			case 0x23: //read format capacities
			{
				//Send back what it wants
				unsigned char response[12];
				
				response[3] = 0x08; //capacity list liength
				response[6] = 0x10; //number of blocks (sectors) (2MB)
				response[8] = 0x01; //reserved/descriptor code (maximum unformatted memory)
				response[10] = 0x02; //block length (512 bytes/sector)
				
				//Send it off
				USB_SendBulkData(0x01, response, 12);
				break;
			}
			case 0x25: //read capacity
			{
				//Send back what it wants
				unsigned char response[8] = {0};
				
				response[2] = 0x0F; //last logical block address
				response[3] = 0xFF;
				response[6] = 0x02; //block length (512 bytes/sector)
				
				//Send it off
				USB_SendBulkData(0x01, response, 8);
				break;
			}
			case 0x1A: //mode sense?
			{
				//Send back what it wants
				unsigned char response[8] = {0};
				
				response[0] = 0x12;
				response[7] = 0x1C;
				
				//Send it off
				USB_SendBulkData(0x01, response, 8);
				break;
			}
			case 0x28: //read sector
			{
				//Send back a sector
				unsigned long long int baseLBA = (buffer[18] | (buffer[17] << 8)) * 0x10000;
				baseLBA += (buffer[20] | (buffer[19] << 8));
				unsigned char response[512];
				unsigned int sectors = dataTransferLength / 512;
				unsigned int i, j;
				for (i = 0; i < sectors; i++)
				{
					//Default the data to all 0xFFs
					for (j = 0; j < 512; j++)
						response[j] = 0xFF;

					//Handle each sector request
					if (h_readSector != NULL)
						memcpy(response, h_readSector(baseLBA), 512);
					baseLBA++;

					USB_SendBulkData(0x01, response, 512);
				}
				break;
			}
			case 0x2A: //write sector
			{
				//Handle this new sector data
				unsigned long long int baseLBA = (buffer[18] | (buffer[17] << 8)) * 0x10000;
				baseLBA += (buffer[20] | (buffer[19] << 8));
				unsigned char data[dataTransferLength];

				MassStorage_ReceiveData(data, dataTransferLength);

				//Figure out how many sectors this is
				unsigned int sectors = dataTransferLength / 512;
				unsigned int i;
				for (i = 0; i < sectors; i++)
				{
					//Handle each sector
					if (h_writeSector != NULL)
						h_writeSector(baseLBA, data+(i*512));
					baseLBA++;
				}
				break;
			}
			default:
			{
				//Uh? Just do whatever's indicated and hope the host is happy...
				if (dataTransferLength > 0)
				{
					//Data's supposed to go somewhere...do we send or receive?
					if (dataExpectedBack)
					{
						//We send back, so echo back garbage
						USB_SendBulkData(0x01, buffer, dataTransferLength);
					}
					else
					{
						//We receive, so get whatever
						void* ptr = malloc(dataTransferLength);
						MassStorage_ReceiveData(ptr, dataTransferLength);
						free(ptr);
						ptr = NULL;
					}
				}
				break;
			}
		}
		
		//Send back a Command Status Wrapper
		unsigned char csw[13] = {0};
		csw[0] = 'U';
		csw[1] = 'S';
		csw[2] = 'B';
		csw[3] = 'S';
		memcpy(csw+4, buffer+4, 4);
		USB_SendBulkData(0x01, csw, 13);
	}
}
