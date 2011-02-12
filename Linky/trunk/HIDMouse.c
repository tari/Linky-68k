#include "HIDMouse.h"

//Descriptors
const unsigned char HIDMouse_DeviceDescriptor[] = {0x12, 0x01, 0x10, 0x01, 0x03, 0x01, 0x02, 0x08, 0x3C, 0x41, 0x10, 0x30, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01};
const unsigned char HIDMouse_ConfigDescriptor[] = {0x09, 0x02, 0x22, 0x00, 0x01, 0x01, 0x00, 0xA0, 0x00, 0x09, 0x04, 0x01, 0x00, 0x01, 0x03, 0x01, 0x02, 0x00,
                           0x09, 0x21, 0x01, 0x01, 0x00, 0x01, 0x22, 0x32, 0x00, 0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x0A};
const unsigned char HIDMouse_ReportDescriptor[] = {0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00,
                           0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x01, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31,
                           0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x06, 0xC0, 0xC0};

unsigned char HIDMouse_Sensitivity;

//Internal driver use
USBPeripheral periph;
INT_HANDLER saved_int_1;
INT_HANDLER saved_int_5;
unsigned int buttonsPressed;

void HIDMouse_HandleSetConfiguration(void)
{
	//Set up the outgoing interrupt pipe
	USB_SetupOutgoingPipe(0x01, Type_Interrupt, 0x08);
}

int HIDMouse_UnknownControlRequest(unsigned char bmRequestType, unsigned char bRequest, unsigned int wValue, unsigned int wIndex, unsigned int wLength)
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
							int length = (sizeof(HIDMouse_ReportDescriptor) > wLength)? wLength : sizeof(HIDMouse_ReportDescriptor);
							USB_StartControlOutput(HIDMouse_ReportDescriptor, length);
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

USBPeripheral HIDMouse_GetInterface(void)
{
  USBPeripheral ret = DEFAULT_USB_PERIPHERAL;

  //Set descriptor information
  ret.deviceDescriptor = HIDMouse_DeviceDescriptor;
  ret.configDescriptor = HIDMouse_ConfigDescriptor;

	//Set callbacks
  ret.h_setConfig = HIDMouse_HandleSetConfiguration;
  ret.h_unknownControlRequest = HIDMouse_UnknownControlRequest;

  return ret;
}

void HIDMouse_Initialize(void)
{
	//Set default values
	buttonsPressed = 0x00;
	HIDMouse_Sensitivity = 0x04;

	periph = HIDMouse_GetInterface();
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

void HIDMouse_Kill(void)
{
	//Cut power to the port
	USB_PeripheralKill();

	//Restore AUTO_INT_1 and AUTO_INT_5 interrupts
  SetIntVec(AUTO_INT_1, saved_int_1);
  SetIntVec(AUTO_INT_5, saved_int_5);
}

unsigned char HIDMouse_GetButtonValue(void)
{
	unsigned char ret;
	
	ret = _keytest(RR_2ND)? 0x01 : 0x00;
	ret |= _keytest(RR_SHIFT)? 0x02 : 0x00;
	
	return ret;
}

void HIDMouse_Do(void)
{
	int keysPressed = 0;

	BEGIN_KEYTEST
	if (_keytest_optimized(RR_UP) || _keytest_optimized(RR_DOWN) ||
	  _keytest_optimized(RR_LEFT) || _keytest_optimized(RR_RIGHT))
	  keysPressed = 1;
	
	if (keysPressed || buttonsPressed != HIDMouse_GetButtonValue())
	{
		unsigned char buffer[3];

		//Figure out what to send
		buttonsPressed = HIDMouse_GetButtonValue();
		buffer[0] = buttonsPressed;
		buffer[1] = _keytest_optimized(RR_RIGHT)? HIDMouse_Sensitivity : 0x00;
		buffer[1] = _keytest_optimized(RR_LEFT)? -HIDMouse_Sensitivity : buffer[1];
		buffer[2] = _keytest_optimized(RR_DOWN)? HIDMouse_Sensitivity : 0x00;
		buffer[2] = _keytest_optimized(RR_UP)? -HIDMouse_Sensitivity : buffer[2];
	
		USB_SendInterruptData(0x01, buffer, 3);
	}
	END_KEYTEST
}
