#include <tigcclib.h>
#include "usb.h"
#include "HIDMouse.h"
#include "HIDKeyboard.h"
#include "SilentLink.h"
#include "MassStorage.h"

void _main(void)
{
	//Pick the driver to load
  HANDLE h = PopupNew("Select Driver", 0);
  if (h == H_NULL) return;
  if (PopupAddText(h, -1, "Silent Link", 1) == H_NULL) return;
  if (PopupAddText(h, -1, "HID Mouse", 2) == H_NULL) return;
  if (PopupAddText(h, -1, "HID Keyboard", 3) == H_NULL) return;
  if (PopupAddText(h, -1, "Mass Storage", 4) == H_NULL) return;
  short ID = PopupDo(h, CENTER, CENTER, 0);
  HeapFree(h);
  if (ID <= 0) return;
  
	//Initialize the driver
  Driver_Initialize();

	//Display a message to the user
  clrscr();
  printf("Connect a USB cable to\n");
  printf("your calculator now.\n\n");
  printf("Press [ESC] to quit.\n\n");

	switch(ID)
	{
		case 1:
		{
			SilentLink_Initialize();
			break;
		}
		case 2:
		{
			HIDMouse_Initialize();
			break;
		}
		case 3:
		{
			HIDKeyboard_Initialize();
			break;
		}
		case 4:
		{
			MassStorage_Initialize();
			break;
		}
		default:
		{
			return;
		}
	}

	//Main key loop
	while(1)
	{
		if (_keytest(RR_ESC))
			break;

		switch(ID)
		{
			case 1:
			{
				SilentLink_Do();
				break;
			}
			case 2:
			{
				if (_keytest(RR_PLUS))
				{
					HIDMouse_Sensitivity++;
					printf("Changed sensitivity to %02u\n", HIDMouse_Sensitivity);
				}
				else if (_keytest(RR_MINUS))
				{
					HIDMouse_Sensitivity--;
					printf("Changed sensitivity to %02u\n", HIDMouse_Sensitivity);
				}
				
				HIDMouse_Do();
				break;
			}
			case 3:
			{
				HIDKeyboard_Do();
				break;
			}
			case 4:
			{
				MassStorage_Do();
				break;
			}
		}
	}

	switch(ID)
	{
		case 1:
		{
			SilentLink_Kill();
			break;
		}
		case 2:
		{
			HIDMouse_Kill();
			break;
		}
		case 3:
		{
			HIDKeyboard_Kill();
			break;
		}
		case 4:
		{
			MassStorage_Kill();
			break;
		}
	}
	
	//Shut down the driver
	Driver_Kill();

	//Flush the keyboard buffer
	GKeyFlush();
}
