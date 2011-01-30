#include "MassStorage.h"

const unsigned char deviceDescriptor[] = {0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x81, 0x07, 0x50, 0x51, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01};
const unsigned char configDescriptor[] = {0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0xE0, 0x00, 0x09, 0x04, 0x00, 0x00, 0x02, 0x08, 0x06, 0x50, 0x00,
                           0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00, 0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00};
const unsigned char maxLUN[] = {0x00};

USBPeripheral periph;
INT_HANDLER saved_int_1;
INT_HANDLER saved_int_5;

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

USBPeripheral MassStorage_GetInterface()
{
  USBPeripheral ret = DEFAULT_USB_PERIPHERAL;

  //Set descriptor information
  ret.deviceDescriptor = deviceDescriptor;
  ret.configDescriptor = configDescriptor;

	//Set callbacks
  ret.h_unknownControlRequest = MassStorage_UnknownControlRequest;

  return ret;
}

void MassStorage_Initialize()
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
}

void MassStorage_Kill()
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

void MassStorage_Do()
{
	if (USB_IsDataReady(0x02))
	{
		unsigned char buffer[4];
		MassStorage_ReceiveData(buffer, 4);

		printf("Received: %02X%02X%02X%02X\n", buffer[0], buffer[1], buffer[2], buffer[3]);
	}
}
