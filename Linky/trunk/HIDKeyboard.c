#include "HIDKeyboard.h"

//Descriptors
const unsigned char HIDKeyboard_DeviceDescriptor[] = {0x12, 0x01, 0x10, 0x01, 0x03, 0x01, 0x01, 0x08, 0x3C, 0x41, 0x10, 0x31, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01};
const unsigned char HIDKeyboard_ReportDescriptor[] = {0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
														0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
														0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0};
const unsigned char HIDKeyboard_ConfigDescriptor[] = {0x09, 0x02, 0x22, 0x00, 0x01, 0x01, 0x00, 0xA0, 0x00, 0x09, 0x04, 0x01, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00,
                           0x09, 0x21, 0x01, 0x01, 0x00, 0x01, 0x22,
                           sizeof(HIDKeyboard_ReportDescriptor) & 0xFF,
                           sizeof(HIDKeyboard_ReportDescriptor) >> 8,
                           0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x0A};

//Internal driver use
USBPeripheral periph;
INT_HANDLER saved_int_1;
INT_HANDLER saved_int_5;

void HIDKeyboard_HandleSetConfiguration(void)
{
	//Set up the outgoing interrupt pipe
	USB_SetupOutgoingPipe(0x01, Type_Interrupt, 0x08);
}

int HIDKeyboard_UnknownControlRequest(unsigned char bmRequestType, unsigned char bRequest, unsigned int wValue, unsigned int wIndex, unsigned int wLength)
{
	int handled = 0;

	switch(bmRequestType)
	{
		case 0x81:
		{
			switch(bRequest)
			{
				case 0x06:
				{
					switch(wValue >> 8)
					{
						case 0x22:
						{
							//Return the HID report descriptor
							int length = (sizeof(HIDKeyboard_ReportDescriptor) > wLength)? wLength : sizeof(HIDKeyboard_ReportDescriptor);
							USB_StartControlOutput(HIDKeyboard_ReportDescriptor, length);
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
			break;
		}
		default:
			break;
	}

	return handled;
}

USBPeripheral HIDKeyboard_GetInterface(void)
{
  USBPeripheral ret = DEFAULT_USB_PERIPHERAL;

  //Set descriptor information
  ret.deviceDescriptor = HIDKeyboard_DeviceDescriptor;
  ret.configDescriptor = HIDKeyboard_ConfigDescriptor;

	//Set callbacks
  ret.h_setConfig = HIDKeyboard_HandleSetConfiguration;
  ret.h_unknownControlRequest = HIDKeyboard_UnknownControlRequest;

  return ret;
}

void HIDKeyboard_Initialize(void)
{
	periph = HIDKeyboard_GetInterface();
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

void HIDKeyboard_Kill(void)
{
	//Cut power to the port
	USB_PeripheralKill();

	//Restore AUTO_INT_1 and AUTO_INT_5 interrupts
  SetIntVec(AUTO_INT_1, saved_int_1);
  SetIntVec(AUTO_INT_5, saved_int_5);
}

void HIDKeyboard_Do(void)
{
	unsigned char keysPressed = 0;
	
	if (_keytest(RR_APPS)) keysPressed = 1;

	if (keysPressed)
	{
		unsigned char buffer[8] = {0};
		
		buffer[2] = 0x04;

		//Figure out what to send
		USB_SendInterruptData(0x01, buffer, 8);
		
		buffer[2] = 0x00;
		USB_SendInterruptData(0x01, buffer, 8);
	}
}
