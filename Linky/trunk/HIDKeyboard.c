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

#define MAX_OUTPUT_KEYS (6+2)

// 1st row
#define COLUMN_ALPHA    (0x80)
#define COLUMN_DIAMOND  (0x40)
#define COLUMN_SHIFT    (0x20)
#define COLUMN_2nd      (0x10)
#define COLUMN_RIGHT    (0x08)
#define COLUMN_DOWN     (0x04)
#define COLUMN_LEFT     (0x02)
#define COLUMN_UP       (0x01)
// 2nd row
#define COLUMN_F5       (0x80)
#define COLUMN_CLEAR    (0x40)
#define COLUMN_CARET    (0x20)
#define COLUMN_SLASH    (0x10)
#define COLUMN_TIMES    (0x08)
#define COLUMN_MINUS    (0x04)
#define COLUMN_PLUS     (0x02)
#define COLUMN_ENTER    (0x01)
// 3rd row
#define COLUMN_F4       (0x80)
#define COLUMN_BKSPC    (0x40)
#define COLUMN_T        (0x20)
#define COLUMN_COMMA    (0x10)
#define COLUMN_9        (0x08)
#define COLUMN_6        (0x04)
#define COLUMN_3        (0x02)
#define COLUMN_SIGN     (0x01)
// 4th row
#define COLUMN_F3       (0x80)
#define COLUMN_CATALOG  (0x40)
#define COLUMN_Z        (0x20)
#define COLUMN_RPAREN   (0x10)
#define COLUMN_8        (0x08)
#define COLUMN_5        (0x04)
#define COLUMN_2        (0x02)
#define COLUMN_DOT      (0x01)
// 5th row
#define COLUMN_F2       (0x80)
#define COLUMN_MODE     (0x40)
#define COLUMN_Y        (0x20)
#define COLUMN_LPAREN   (0x10)
#define COLUMN_7        (0x08)
#define COLUMN_4        (0x04)
#define COLUMN_1        (0x02)
#define COLUMN_0        (0x01)
// 6th row
#define COLUMN_F1       (0x80)
#define COLUMN_HOME     (0x40)
#define COLUMN_X        (0x20)
#define COLUMN_EQUAL    (0x10)
#define COLUMN_WITH     (0x08)
#define COLUMN_EE       (0x04)
#define COLUMN_STO      (0x02)
#define COLUMN_APPS     (0x01)
// 7th row
#define COLUMN_ESC      (0x01)



void HIDKeyboard_Do(void)
{
	unsigned char keysPressed = 0;
	unsigned char new_keyboard_state[NR_ROWS];
	unsigned char output[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	unsigned char modifier_keys;
	unsigned char current_row;
	unsigned short i = 0, j = 1;

	unsigned short nr_keys = 0+2;

	//! Queue up to MAX_OUTPUT_KEYS, drop excess keys.
	inline unsigned char QueueKey(unsigned char key)
	{
		keysPressed = 1;
		if (nr_keys < MAX_OUTPUT_KEYS)
		{
			output[nr_keys++] = key;
		}
	}


	// 0) Read all 89T keyboard rows.
	for (i = 0; i < NR_ROWS; i++)
	{
		new_keyboard_state[i] = _rowread_inverted(j);
		j <<= 1;
	}


	// 1) Handle first row: ALPHA, DIAMOND, SHIFT, 2nd, RIGHT, DOWN, LEFT, UP
	current_row = new_keyboard_state[0];
	modifier_keys = current_row;

	if (modifier_keys & COLUMN_DIAMOND)      // Map DIAMOND to LEFT CTRL
	{
		output[0] |= 1;
		keysPressed = 1;
	}
	if (modifier_keys & COLUMN_SHIFT)        // Map SHIFT to LEFT CTRL
	{
		output[0] |= 2;
		keysPressed = 1;
		// TODO trigger alpha mode ?
	}
	if (modifier_keys & COLUMN_2nd)          // Map 2nd to LEFT ALT
	{
		output[0] |= 4;
		keysPressed = 1;
	}
	if (modifier_keys & COLUMN_ALPHA)
	{
		// TODO trigger alpha mode.
	}

	if (modifier_keys & COLUMN_RIGHT)        // Map RIGHT to Keyboard RightArrow
	{
		QueueKey(0x4F);
	}
	if (modifier_keys & COLUMN_DOWN)         // Map DOWN to Keyboard DownArrow
	{
		QueueKey(0x51);
	}
	if (modifier_keys & COLUMN_LEFT)         // Map LEFT to Keyboard LeftArrow
	{
		QueueKey(0x50);
	}
	if (modifier_keys & COLUMN_UP)           // Map UP to Keyboard UpArrow
	{
		QueueKey(0x52);
	}



	// 2) Handle second row: F5, CLEAR, ^, /, *, -, +, ENTER
	current_row = new_keyboard_state[1];
	if (current_row & COLUMN_F5)             // Map F5 to F5
	{
		QueueKey(0x3E);
	}
	if (current_row & COLUMN_CLEAR)          // ?
	{
		// Nothing for now.
	}
	if (current_row & COLUMN_CARET)          // ?
	{
		// Nothing for now.
	}
	if (current_row & COLUMN_SLASH)          // Map / to Keypad /
	{
		QueueKey(0x54);
	}
	if (current_row & COLUMN_TIMES)          // Map * to Keypad *
	{
		QueueKey(0x55);
	}
	if (current_row & COLUMN_MINUS)          // Map - to Keypad -
	{
		QueueKey(0x56);
	}
	if (current_row & COLUMN_PLUS)           // Map + to Keypad +
	{
		QueueKey(0x57);
	}
	if (current_row & COLUMN_ENTER)          // Map ENTER to Keyboard Return
	{
		QueueKey(0x28);
	}



	// 3) Handle third row: F4, BkSpc, T, comma, 9, 6, 3, (-)
	current_row = new_keyboard_state[2];
	if (current_row & COLUMN_F4)             // Map F4 to F4
	{
		QueueKey(0x3D);
	}
	if (current_row & COLUMN_BKSPC)          // Map Bkspc to Keyboard DELETE (Backspace)
	{
		QueueKey(0x2A);
	}
	if (current_row & COLUMN_T)              // Map T to Keyboard t / T
	{
		QueueKey(0x17);
	}
	if (current_row & COLUMN_COMMA)          // Map , to Keyboard ,
	{
		QueueKey(0x36);
	}
	if (current_row & COLUMN_9)              // Map 9 to Keypad 9
	{
		QueueKey(0x61);
	}
	if (current_row & COLUMN_6)              // Map 6 to Keypad 6
	{
		QueueKey(0x5E);
	}
	if (current_row & COLUMN_3)              // Map 3 to Keypad 3
	{
		QueueKey(0x5B);
	}
	if (current_row & COLUMN_SIGN)           // ?
	{
		// Nothing for now.
	}



	// 4) Handle fourth row: F3, CATALOG, Z, ), 8, 5, 2, .
	current_row = new_keyboard_state[3];
	if (current_row & COLUMN_F3)             // Map F3 to F3
	{
		QueueKey(0x3C);
	}
	if (current_row & COLUMN_CATALOG)        // ?
	{
		// Nothing for now.
	}
	if (current_row & COLUMN_Z)              // Map Z to Keyboard z / Z
	{
		QueueKey(0x1D);
	}
	if (current_row & COLUMN_RPAREN)         // TODO
	{
		// TODO
	}
	if (current_row & COLUMN_8)              // Map 8 to Keypad 8
	{
		QueueKey(0x60);
	}
	if (current_row & COLUMN_5)              // Map 5 to Keypad 5
	{
		QueueKey(0x5D);
	}
	if (current_row & COLUMN_2)              // Map 2 to Keypad 2
	{
		QueueKey(0x5A);
	}
	if (current_row & COLUMN_DOT)            // Map . to Keyboard .
	{
		QueueKey(0x37);
	}



	// 5) Handle fifth row: F2, MODE, Y, (, 7, 4, 1, 0
	current_row = new_keyboard_state[4];
	if (current_row & COLUMN_F2)             // Map F2 to F2
	{
		QueueKey(0x3B);
	}
	if (current_row & COLUMN_MODE)           // ?
	{
		// Nothing for now.
	}
	if (current_row & COLUMN_Y)              // Map Y to Keyboard y / Y
	{
		QueueKey(0x1C);
	}
	if (current_row & COLUMN_LPAREN)         // TODO
	{
		// TODO
	}
	if (current_row & COLUMN_7)              // Map 7 to Keypad 7
	{
		QueueKey(0x5F);
	}
	if (current_row & COLUMN_4)              // Map 4 to Keypad 4
	{
		QueueKey(0x5C);
	}
	if (current_row & COLUMN_1)              // Map 1 to Keypad 1
	{
		QueueKey(0x59);
	}
	if (current_row & COLUMN_0)              // Map 0 to Keypad 0
	{
		QueueKey(0x62);
	}



	// 6) Handle sixth row: F1, HOME, X, =, |, EE, STO, APPS
	current_row = new_keyboard_state[5];
	if (current_row & COLUMN_F1)             // Map F1 to F1
	{
		QueueKey(0x3A);
	}
	if (current_row & COLUMN_HOME)           // ?
	{
		// Nothing for now.
	}
	if (current_row & COLUMN_X)              // Map X to Keyboard x / X
	{
		QueueKey(0x1B);
	}
	if (current_row & COLUMN_EQUAL)          // Map = to Keyboard =
	{
		QueueKey(0x2E);
	}
	if (current_row & COLUMN_WITH)           // TODO
	{
		// TODO
	}
	if (current_row & COLUMN_EE)             // ?
	{
		// Nothing for now.
	}
	if (current_row & COLUMN_STO)            // Map Sto to Keyboard Tab
	{
		QueueKey(0x2B);
	}
	if (current_row & COLUMN_APPS)           // ?
	{
		// Nothing for now.
	}



	// 7) Handle seventh row: ESC
	current_row = new_keyboard_state[6];
	if (current_row & COLUMN_ESC)            // Map ESC to Keyboard ESCAPE
	{
		QueueKey(0x29);
	}



	// 8) Finally, send current key state
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
