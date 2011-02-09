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


#define SET_LEFT_CTRL   {output[0] |=  1;}
#define SET_LEFT_SHIFT  {output[0] |=  2;}
#define SET_LEFT_ALT    {output[0] |=  4;}
#define SET_LEFT_GUI    {output[0] |=  8;}
#define CLR_LEFT_CTRL   {output[0] &= ~1;}
#define CLR_LEFT_SHIFT  {output[0] &= ~2;}
#define CLR_LEFT_ALT    {output[0] &= ~4;}
#define CLR_LEFT_GUI    {output[0] &= ~8;}

#define IS_ALPHA_PRESSED    modifier_keys & COLUMN_ALPHA
#define IS_DIAMOND_PRESSED  modifier_keys & COLUMN_DIAMOND
#define IS_SHIFT_PRESSED    modifier_keys & COLUMN_SHIFT
#define IS_2nd_PRESSED      modifier_keys & COLUMN_2nd

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
	inline __attribute__((regparm(2))) void QueueKey(unsigned char normalkey, unsigned char alphakey)
	{
		keysPressed = 1;
		if (nr_keys < MAX_OUTPUT_KEYS)
		{
			// Trigger alpha mode if ALPHA or SHIFT is pressed.
			if ((IS_ALPHA_PRESSED) || (IS_SHIFT_PRESSED))
			{
				output[nr_keys++] = alphakey;
			}
			else
			{
				output[nr_keys++] = normalkey;
			}
		}
	}


	// 0) Read all 89T keyboard rows.
	for (i = 0; i < NR_ROWS; i++)
	{
		new_keyboard_state[i] = _rowread_inverted(j);
		j <<= 1;
	}


	// 1) Handle first row: ALPHA, DIAMOND, SHIFT, 2nd, RIGHT, DOWN, LEFT, UP
	//    Generate several special keys.
	current_row = new_keyboard_state[0];
	modifier_keys = current_row;

	if (IS_DIAMOND_PRESSED)                   // Map DIAMOND to LEFT CTRL
	{
		SET_LEFT_CTRL
		keysPressed = 1;
	}
	if (IS_SHIFT_PRESSED)                    // Map SHIFT to LEFT SHIFT
	{
		SET_LEFT_SHIFT
		keysPressed = 1;
		// This triggers alpha mode, in QueueKey.
	}
	if (IS_2nd_PRESSED)                      // Map 2nd to LEFT ALT
	{
		SET_LEFT_ALT
		keysPressed = 1;
	}
	if (IS_ALPHA_PRESSED)
	{
		// This triggers alpha mode, in QueueKey.
	}

	if (modifier_keys & COLUMN_RIGHT)        // Map RIGHT to Keyboard RightArrow, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate Keyboard End for 2nd + RIGHT.
			CLR_LEFT_ALT
			QueueKey(0x4D, 0x4D);
		}
		else
		{
			QueueKey(0x4F, 0x4F);
		}
	}
	if (modifier_keys & COLUMN_DOWN)         // Map DOWN to Keyboard DownArrow, unless...
	{
		if (IS_DIAMOND_PRESSED)
		{
			// Generate Keyboard End forward for DIAMOND + DOWN.
			QueueKey(0x4D, 0x4D);
			// Do not clear left Ctrl.
		}
		else if (IS_2nd_PRESSED)
		{
			// Generate Keyboard PageDown for 2nd + DOWN.
			CLR_LEFT_ALT
			QueueKey(0x4E, 0x4E);
		}
		else
		{
			QueueKey(0x51, 0x51);
		}
	}
	if (modifier_keys & COLUMN_LEFT)         // Map LEFT to Keyboard LeftArrow, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate Keyboard Home for 2nd + LEFT.
			CLR_LEFT_ALT
			QueueKey(0x4A, 0x4A);
		}
		else
		{
			QueueKey(0x50, 0x50);
		}
	}
	if (modifier_keys & COLUMN_UP)           // Map UP to Keyboard UpArrow, unless...
	{
		if (IS_DIAMOND_PRESSED)
		{
			// Generate Keyboard Home forward for DIAMOND + UP.
			QueueKey(0x4A, 0x4A);
			// Do not clear left Ctrl.
		}
		else if (IS_2nd_PRESSED)
		{
			// Generate Keyboard PageUp for 2nd + UP.
			CLR_LEFT_ALT
			QueueKey(0x4B, 0x4B);
		}
		else
		{
			QueueKey(0x52, 0x52);
		}
	}



	// 2) Handle second row: F5, CLEAR, ^, /, *, -, +, ENTER
	current_row = new_keyboard_state[1];
	if (current_row & COLUMN_F5)             // Map F5 to F5, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate Keyboard F10 for 2nd + F5
			CLR_LEFT_ALT
			QueueKey(0x43, 0x43);
		}
		else
		{
			QueueKey(0x3E, 0x3E);
		}
	}
	if (current_row & COLUMN_CLEAR)          // ?
	{
		// Nothing for now.
	}
	if (current_row & COLUMN_CARET)          // ?
	{
		// Nothing for now.
	}
	if (current_row & COLUMN_SLASH)          // Map / to Keypad / | Keyboard e / E, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate ] in reaction to 2nd + /
			CLR_LEFT_ALT
			QueueKey(0x30, 0x30);
		}
		else
		{
			QueueKey(0x54, 0x08);
		}
	}
	if (current_row & COLUMN_TIMES)          // Map * to Keypad * | Keyboard j / J
	{
		QueueKey(0x55, 0x0D);
	}
	if (current_row & COLUMN_MINUS)          // Map - to Keypad - | Keyboard o / O
	{
		QueueKey(0x56, 0x12);
	}
	if (current_row & COLUMN_PLUS)           // Map + to Keypad + | Keyboard u / U
	{
		QueueKey(0x57, 0x18);
	}
	if (current_row & COLUMN_ENTER)          // Map ENTER to Keyboard Return
	{
		QueueKey(0x28, 0x28);
	}



	// 3) Handle third row: F4, BkSpc, T, comma, 9, 6, 3, (-)
	current_row = new_keyboard_state[2];
	if (current_row & COLUMN_F4)             // Map F4 to F4, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate Keyboard F9 for 2nd + F4
			CLR_LEFT_ALT
			QueueKey(0x42, 0x42);
		}
		else
		{
			QueueKey(0x3D, 0x3D);
		}
	}
	if (current_row & COLUMN_BKSPC)          // Map Bkspc to Keyboard DELETE (Backspace), unless...
	{
		if (IS_DIAMOND_PRESSED)
		{
			// Generate Keyboard delete forward for DIAMOND + BkSpc.
			CLR_LEFT_CTRL
			QueueKey(0x4C, 0x4C);
		}
		else if (IS_2nd_PRESSED)
		{
			// Generate Keyboard Insert for 2nd + BkSpc.
			CLR_LEFT_ALT
			QueueKey(0x49, 0x49);
		}
		else
		{
			QueueKey(0x2A, 0x2A);
		}
	}
	if (current_row & COLUMN_T)              // Map T to Keyboard t / T
	{
		QueueKey(0x17, 0x17);
	}
	if (current_row & COLUMN_COMMA)          // Map , to Keyboard , | Keyboard d / D, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate [ in reaction to 2nd + ,
			CLR_LEFT_ALT
			QueueKey(0x2F, 0x2F);
		}
		else
		{
			QueueKey(0x36, 0x07);
		}
	}
	if (current_row & COLUMN_9)              // Map 9 to Keypad 9 | Keyboard i / I, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate ; in reaction to 2nd + *
			CLR_LEFT_ALT
			QueueKey(0x33, 0x33);
		}
		else
		{
			QueueKey(0x61, 0x0C);
		}
	}
	if (current_row & COLUMN_6)              // Map 6 to Keypad 6 | Keyboard n / N
	{
		QueueKey(0x5E, 0x11);
	}
	if (current_row & COLUMN_3)              // Map 3 to Keypad 3 | Keyboard s / S
	{
		QueueKey(0x5B, 0x16);
	}
	if (current_row & COLUMN_SIGN)           // Map (-) to Keyboard Spacebar
	{
		QueueKey(0x2C, 0x2C);
	}



	// 4) Handle fourth row: F3, CATALOG, Z, ), 8, 5, 2, .
	current_row = new_keyboard_state[3];
	if (current_row & COLUMN_F3)             // Map F3 to F3, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate Keyboard F8 for 2nd + F3
			CLR_LEFT_ALT
			QueueKey(0x41, 0x41);
		}
		else
		{
			QueueKey(0x3C, 0x3C);
		}
	}
	if (current_row & COLUMN_CATALOG)        // ?
	{
		// Nothing for now.
	}
	if (current_row & COLUMN_Z)              // Map Z to Keyboard z / Z
	{
		QueueKey(0x1D, 0x1D);
	}
	if (current_row & COLUMN_RPAREN)         // Map ) to Keypad ) | Keyboard c / C, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate } in reaction to 2nd + )
			// The } character needs the shift modifier.
			SET_LEFT_SHIFT
			CLR_LEFT_ALT
			QueueKey(0x30, 0x30);
		}
		else
		{
			// The ) character needs the shift modifier. Set it if alpha or shift are not pressed.
			if (!((IS_ALPHA_PRESSED) || (IS_SHIFT_PRESSED)))
			{
				SET_LEFT_SHIFT
			}
			QueueKey(0x27, 0x06);
		}
	}
	if (current_row & COLUMN_8)              // Map 8 to Keypad 8 | Keyboard h / H
	{
		QueueKey(0x60, 0x0B);
	}
	if (current_row & COLUMN_5)              // Map 5 to Keypad 5 | Keyboard m / M
	{
		QueueKey(0x5D, 0x10);
	}
	if (current_row & COLUMN_2)              // Map 2 to Keypad 2 | Keyboard r / R, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate \ in reaction to 2nd + 2
			CLR_LEFT_ALT
			QueueKey(0x31, 0x31);
		}
		else
		{
			QueueKey(0x5A, 0x15);
		}
	}
	if (current_row & COLUMN_DOT)            // Map . to Keyboard . | Keyboard w / W, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate > in reaction to 2nd + .
			// The > character needs the shift modifier.
			SET_LEFT_SHIFT
			CLR_LEFT_ALT
			QueueKey(0x37, 0x37);
		}
		else
		{
			QueueKey(0x37, 0x1A);
		}
	}



	// 5) Handle fifth row: F2, MODE, Y, (, 7, 4, 1, 0
	current_row = new_keyboard_state[4];
	if (current_row & COLUMN_F2)             // Map F2 to F2, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate Keyboard F7 for 2nd + F2
			CLR_LEFT_ALT
			QueueKey(0x40, 0x40);
		}
		else
		{
			QueueKey(0x3B, 0x3B);
		}
	}
	if (current_row & COLUMN_MODE)           // ?
	{
		// Nothing for now. May map underscore.
	}
	if (current_row & COLUMN_Y)              // Map Y to Keyboard y / Y
	{
		QueueKey(0x1C, 0x1C);
	}
	if (current_row & COLUMN_LPAREN)         // Map ( to Keypad ( | Keyboard b / B, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate { in reaction to 2nd + (
			// The { character needs the shift modifier.
			SET_LEFT_SHIFT
			CLR_LEFT_ALT
			QueueKey(0x2F, 0x2F);
		}
		else
		{
			// The ( character needs the shift modifier. Set it if alpha or shift are not pressed.
			if (!((IS_ALPHA_PRESSED) || (IS_SHIFT_PRESSED)))
			{
				SET_LEFT_SHIFT
			}
			QueueKey(0x26, 0x05);
		}
	}
	if (current_row & COLUMN_7)              // Map 7 to Keypad 7 | Keyboard g / G
	{
		QueueKey(0x5F, 0x0A);
	}
	if (current_row & COLUMN_4)              // Map 4 to Keypad 4 | Keyboard l / L, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate : in reaction to 2nd + 4
			// The : character needs the shift modifier.
			SET_LEFT_SHIFT
			CLR_LEFT_ALT
			QueueKey(0x33, 0x33);
		}
		else
		{
			QueueKey(0x5C, 0x0F);
		}
	}
	if (current_row & COLUMN_1)              // Map 1 to Keypad 1 | Keyboard q / Q, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate " in reaction to 2nd + 1
			// The : character needs the shift modifier.
			SET_LEFT_SHIFT
			CLR_LEFT_ALT
			QueueKey(0x34, 0x34);
		}
		else
		{
			QueueKey(0x59, 0x14);
		}
	}
	if (current_row & COLUMN_0)              // Map 0 to Keypad 0 | Keyboard v / V, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate < in reaction to 2nd + 0
			// The < character needs the shift modifier.
			SET_LEFT_SHIFT
			CLR_LEFT_ALT
			QueueKey(0x36, 0x36);
		}
		else
		{
			QueueKey(0x62, 0x19);
		}
	}



	// 6) Handle sixth row: F1, HOME, X, =, |, EE, STO, APPS
	current_row = new_keyboard_state[5];
	if (current_row & COLUMN_F1)             // Map F1 to F1, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate Keyboard F6 for 2nd + F1
			CLR_LEFT_ALT
			QueueKey(0x3F, 0x3F);
		}
		else
		{
			QueueKey(0x3A, 0x3A);
		}
	}
	if (current_row & COLUMN_HOME)           // Map HOME to Keyboard Left GUI (Win / Apple) modifier key.
	{
		SET_LEFT_GUI;
	}
	if (current_row & COLUMN_X)              // Map X to Keyboard x / X
	{
		QueueKey(0x1B, 0x1B);
	}
	if (current_row & COLUMN_EQUAL)          // Map = to Keyboard = | Keyboard a / A, unless...
	{
		if (IS_2nd_PRESSED)
		{
			// Generate ' in reaction to 2nd + =
			CLR_LEFT_ALT
			QueueKey(0x34, 0x34);
		}
		else
		{
			QueueKey(0x2E, 0x04);
		}
	}
	if (current_row & COLUMN_WITH)           // Map | to Keyboard | | Keyboard f / F
	{
		// The pipe character needs the shift modifier. Set it if alpha or shift are not pressed.
		if (!((IS_ALPHA_PRESSED) || (IS_SHIFT_PRESSED)))
		{
			SET_LEFT_SHIFT
		}
		QueueKey(0x31, 0x09);
	}
	if (current_row & COLUMN_EE)             // May EE to Keyboard k / K
	{
		QueueKey(0x0E, 0x0E);
	}
	if (current_row & COLUMN_STO)            // Map Sto to Keyboard Tab | Keyboard p / P
	{
		QueueKey(0x2B, 0x13);
	}
	if (current_row & COLUMN_APPS)           // ?
	{
		// Nothing for now.
	}



	// 7) Handle seventh row: ESC
	current_row = new_keyboard_state[6];
	if (current_row & COLUMN_ESC)            // Map ESC to Keyboard ESCAPE
	{
		QueueKey(0x29, 0x29);
	}



	// 8) Finally, send current key state
	if (keysPressed)
	{
		USB_SendInterruptData(0x01, output, 8);

		// Clear keys and wait for some time before sending key up event.
		_memset(output+2, 0, sizeof(output)-2);
		asm volatile("moveq #-1,%%d0; 0: dbf %%d0,0b" : : : "d0", "cc");

		USB_SendInterruptData(0x01, output, 8);
	}

	memcpy(keyboard_state, new_keyboard_state, sizeof(keyboard_state));

	// Wait for a significant amount of time, so as to reduce duplicate keypresses.
	asm volatile("moveq #-1,%%d0; 0: dbf %%d0,0b; 1: dbf %%d0,1b" : : : "d0", "cc");
}
