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

#define NR_ROWS (7)

unsigned char keyboard_state[NR_ROWS] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

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

#define MAX_OUTPUT_KEYS (6)

#define COLUMN_ALPHA    (0x80)
#define COLUMN_DIAMOND  (0x40)
#define COLUMN_SHIFT    (0x20)
#define COLUMN_2nd      (0x10)
#define COLUMN_RIGHT    (0x08)
#define COLUMN_DOWN     (0x04)
#define COLUMN_LEFT     (0x02)
#define COLUMN_UP       (0x01)

void HIDKeyboard_Do(void)
{
	unsigned char keysPressed = 0;
	unsigned char new_keyboard_state[NR_ROWS];
	unsigned char output[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	unsigned char modifier_keys;
	unsigned short i = 0, j = 1;

	unsigned short nr_keys = 0+2;

	// Read all 89T keyboard rows.
	for (i = 0; i < NR_ROWS; i++)
	{
		new_keyboard_state[i] = _rowread_inverted(j);
		j <<= 1;
	}

	// TODO handle more than modifier keys and UP/DOWN/LEFT/RIGHT
	modifier_keys = new_keyboard_state[0];
	if (modifier_keys)
	{
		keysPressed = 1;
	}

	if (modifier_keys & COLUMN_DIAMOND)      // Map DIAMOND to LEFT CTRL
	{
		output[0] |= 1;
	}
	if (modifier_keys & COLUMN_SHIFT)        // Map SHIFT to LEFT CTRL
	{
		output[0] |= 2;
		// TODO trigger alpha mode ?
	}
	if (modifier_keys & COLUMN_2nd)          // Map 2nd to LEFT ALT
	{
		output[0] |= 4;
	}
	if (modifier_keys & COLUMN_ALPHA)
	{
		// TODO trigger alpha mode.
	}

	if (modifier_keys & COLUMN_RIGHT)        // Map RIGHT to Keyboard RightArrow
	{
		output[nr_keys++] = 0x4F;
	}
	if (modifier_keys & COLUMN_DOWN)         // Map DOWN to Keyboard DownArrow
	{
		output[nr_keys++] = 0x51;
	}
	if (modifier_keys & COLUMN_LEFT)         // Map LEFT to Keyboard LeftArrow
	{
		output[nr_keys++] = 0x50;
	}
	if (modifier_keys & COLUMN_UP)           // Map UP to Keyboard UpArrow
	{
		output[nr_keys++] = 0x52;
	}

	// FIXME: for now, we're handling only four keys, but we'll have to clamp to MAX_OUTPUT_KEYS keys at some point.
	if (keysPressed)
	{
		USB_SendInterruptData(0x01, output, 8);

		memset(output+2, 0, sizeof(output)-2);
		USB_SendInterruptData(0x01, output, 8);
	}

	memcpy(keyboard_state, new_keyboard_state, sizeof(keyboard_state));

	// Wait for a significant amount of time, so as to reduce duplicate keypresses.
	asm volatile("moveq #-1,%%d0; 0: dbf %%d0,0b; 1: dbf %%d0,1b" : : : "d0", "cc");
}
