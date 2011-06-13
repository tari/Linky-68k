#include "SerialAdapter.h"

const unsigned char deviceDescriptor[] = {0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x40, 0x57, 0x05, 0x08, 0x20, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01};
const unsigned char configDescriptor[] = {0x09, 0x02, 0x27, 0x00, 0x01, 0x01, 0x00, 0xA0, 0x00, 0x09, 0x04, 0x00, 0x00, 0x03, 0xFF, 0x00, 0x00, 0x00,
                           0x07, 0x05, 0x81, 0x03, 0x0A, 0x00, 0x01, 0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00,
                           0x07, 0x05, 0x83, 0x02, 0x40, 0x00, 0x00};
const unsigned char ENDPOINT_INCOMING = 0x02; //from peripheral perspective
const unsigned char ENDPOINT_OUTGOING = 0x03; //from peripheral perspective
const unsigned char ENDPOINT_MAX_PACKET_SIZE = 0x40;
const unsigned char* PARITY_NONE = "None";
const unsigned char* PARITY_ODD = "Odd";
const unsigned char* PARITY_EVEN = "Even";
const unsigned char* PARITY_MARK = "Mark";
const unsigned char* PARITY_SPACE = "Space";
unsigned char readValue[1];

USBPeripheral periph;
Handle_ReceivingData h_receivingData;
Handle_IncomingData h_incomingData;

INT_HANDLER saved_int_1;
INT_HANDLER saved_int_5;

void SerialAdapter_SendData(unsigned char* data, unsigned int count)
{
	USB_SendBulkData(ENDPOINT_OUTGOING, data, count);
}

unsigned int SerialAdapter_ReceiveData(unsigned char* buffer, unsigned int count)
{
	unsigned char* dataBuffer = buffer;
	unsigned int bytesToReceive;
	unsigned int bytesReceived = 0;

	while (count > 0)
	{
		if (USB_IsDataReady(ENDPOINT_INCOMING))
		{
			bytesToReceive = count > ENDPOINT_MAX_PACKET_SIZE? ENDPOINT_MAX_PACKET_SIZE : count;
			bytesReceived += USB_ReceiveBulkData(ENDPOINT_INCOMING, dataBuffer, bytesToReceive);
	
			dataBuffer += bytesReceived;
			count -= bytesReceived;
		}
	}
	
	return bytesReceived;
}

int SerialAdapter_UnknownControlRequest(unsigned char bmRequestType, unsigned char bRequest, unsigned int wValue, unsigned int wIndex, unsigned int wLength)
{
	int handled = 0;

	switch(bmRequestType)
	{
		case 0x00:
		{
			switch (bRequest)
			{
				case 0x03: //set feature request
				{
					if (wValue == 0x0001)
					{
						//"Remote Wakeup" device feature
					}
					
					break;
				}
				default:
					break;
			}
			
			break;
		}
		case 0x21:
		{
			switch (bRequest)
			{
				case 0x20: //set line request
				{
					if (wLength == 7)
					{
						unsigned char data[7];
						USB_ReceiveControlData(data, 7);
						USB_FinishControlRequest();
						handled = 1;
						
						//Interpret this line change request
						unsigned long baudRate;
						baudRate = data[0];
						baudRate |= (unsigned long)data[1] << 8;
						baudRate |= (unsigned long)data[2] << 16;
						baudRate |= (unsigned long)data[3] << 24;
						double stopBits = 0;
						switch (data[4])
						{
							case 0:
							{
								stopBits = 1;
								break;
							}
							case 1:
							{
								stopBits = 1.5;
								break;
							}
							case 2:
							{
								stopBits = 2;
								break;
							}
							default:
								break;
						}
						const unsigned char* parity = NULL;
						switch (data[5])
						{
							case 0:
							{
								parity = PARITY_NONE;
								break;
							}
							case 1:
							{
								parity = PARITY_ODD;
								break;
							}
							case 2:
							{
								parity = PARITY_EVEN;
								break;
							}
							case 3:
							{
								parity = PARITY_MARK;
								break;
							}
							case 4:
							{
								parity = PARITY_SPACE;
								break;
							}
							default:
								break;
						}
						//data[6] is apparently something to do with "cflag" and "CSIZE"

						printf("Line change:\n");
						printf("  Baud rate: %4lu\n", baudRate & 0xFFFF);
						printf("  Stop bits: %f\n", stopBits);
						printf("  Parity:    ");
						printf(parity);
						printf("\n");
					}
					else
					{
						//Uh...I don't know...
					}
					
					break;
				}
				case 0x22: //set control request
				{
					//wValue is...something
					break;
				}
				default:
					break;
			}
			
			break;
		}
		case 0x40:
		{
			switch (bRequest)
			{
				case 0x01: //Vendor Write Value request
				{
					//wValue is the "address"/register
					//wIndex is the value (oddly enough)
					break;
				}
				default:
					break;
			}
			
			break;
		}
		case 0xC0:
		{
			switch (bRequest)
			{
				case 0x01: //Vendor Read Value request
				{
					//wValue is the "address"/register
					//wLength should be 1
					if (wLength == 1)
					{
						//Just return the last byte written, whatever it is (garbage)
						USB_StartControlOutput(readValue, 1);
						handled = 1;
					}
					else
					{
						//Uh...I don't know...
					}

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

void SerialAdapter_HandleIncomingData(unsigned char readyMap)
{
	if (USB_IsDataReady(ENDPOINT_INCOMING))
	{
		if (h_receivingData != NULL)
			h_receivingData(1);
	}
}

USBPeripheral SerialAdapter_GetInterface(void)
{
  USBPeripheral ret = DEFAULT_USB_PERIPHERAL;

  //Set descriptor information
  ret.deviceDescriptor = deviceDescriptor;
  ret.configDescriptor = configDescriptor;

	//Set callbacks
  ret.h_unknownControlRequest = SerialAdapter_UnknownControlRequest;
  ret.h_incomingData = SerialAdapter_HandleIncomingData;

  return ret;
}

void SerialAdapter_Initialize(Handle_ReceivingData receivingData)
{
	periph = SerialAdapter_GetInterface();
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
	h_receivingData = receivingData;
	
	USB_SetupOutgoingPipe(ENDPOINT_OUTGOING, Type_Bulk, ENDPOINT_MAX_PACKET_SIZE);
}

void SerialAdapter_Kill(void)
{
	//Cut power to the port
	USB_PeripheralKill();

	//Restore AUTO_INT_1 and AUTO_INT_5 interrupts
	SetIntVec(AUTO_INT_1, saved_int_1);
	SetIntVec(AUTO_INT_5, saved_int_5);
}
