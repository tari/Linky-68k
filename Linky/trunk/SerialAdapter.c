#include "SerialAdapter.h"

const unsigned char deviceDescriptor[] = {0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x40, 0x57, 0x05, 0x08, 0x20, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01};
const unsigned char configDescriptor[] = {0x09, 0x02, 0x27, 0x00, 0x01, 0x01, 0x00, 0xA0, 0x00, 0x09, 0x04, 0x00, 0x00, 0x03, 0xFF, 0x00, 0x00, 0x00,
                           0x07, 0x05, 0x81, 0x03, 0x0A, 0x00, 0x01, 0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00,
                           0x07, 0x05, 0x83, 0x02, 0x40, 0x00, 0x00};
unsigned char readValue[1];

USBPeripheral periph;
Handle_DataReceived h_dataReceived;
Handle_IncomingData h_incomingData;

INT_HANDLER saved_int_1;
INT_HANDLER saved_int_5;

unsigned int SerialAdapter_ReceiveData(unsigned char* buffer, unsigned int count)
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

int SerialAdapter_UnknownControlRequest(unsigned char bmRequestType, unsigned char bRequest, unsigned int wValue, unsigned int wIndex, unsigned int wLength)
{
	int handled = 0;

	switch(bmRequestType)
	{
		case 0xC0:
		{
			switch(bRequest)
			{
				case 0x01:
				{
					//wValue is "address"
					//wIndex is the value (oddly enough)
					//wLength should be 1
					USB_StartControlOutput(readValue, 1);
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

void SerialAdapter_HandleIncomingData(unsigned char readyMap)
{
	if (USB_IsDataReady(0x02))
	{
		unsigned char b[1] = {0};
		SerialAdapter_ReceiveData(b, 1);

		printf("%c", b[0]);
		
		USB_SendBulkData(0x03, b, 1);
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

void SerialAdapter_Initialize(Handle_DataReceived dataReceived)
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
	h_dataReceived = dataReceived;
	
	USB_SetupOutgoingPipe(0x03, Type_Bulk, 0x40);
}

void SerialAdapter_Kill(void)
{
	//Cut power to the port
	USB_PeripheralKill();

	//Restore AUTO_INT_1 and AUTO_INT_5 interrupts
	SetIntVec(AUTO_INT_1, saved_int_1);
	SetIntVec(AUTO_INT_5, saved_int_5);
}

void SerialAdapter_Do(void)
{
	//Do nothing for now
	//unsigned char buf[1];
	//buf[0] = ngetchx();
	//USB_SendBulkData(0x03, buf, 1);
}
