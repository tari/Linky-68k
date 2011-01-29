#include <tigcclib.h>
#include "usb.h"
#include "SilentLink.h"
#include "HIDMouse.h"

void _main(void)
{
	//Pick the driver to load
  HANDLE h = PopupNew("Select Driver", 0);
  if (h == H_NULL) return;
  if (PopupAddText(h, -1, "Silent Link", 1) == H_NULL) return;
  if (PopupAddText(h, -1, "HID Mouse", 2) == H_NULL) return;
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
			USBPeripheral ret = SilentLink_GetInterface();
			Driver_SetPeripheralInterface(&ret);

			//Restart the controller
			USB_KillPower();
			USB_PeripheralInitialize();
			break;
		}
		case 2:
		{
			HIDMouse_Initialize();
			break;
		}
		default:
		{
			return;
		}
	}
	
	INT_HANDLER saved_int_1;
	INT_HANDLER saved_int_5;

	//Save AUTO_INT_1 and AUTO_INT_5 interrupts because they interfere with key reading
	saved_int_1 = GetIntVec(AUTO_INT_1);
	saved_int_5 = GetIntVec(AUTO_INT_5);
	SetIntVec(AUTO_INT_1, DUMMY_HANDLER);
	SetIntVec(AUTO_INT_5, DUMMY_HANDLER);

	//Main key loop
	while(1)
	{
		if (_keytest(RR_ESC))
			break;

		switch(ID)
		{
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
		}
	}

	//Restore AUTO_INT_1 and AUTO_INT_5 interrupts
  SetIntVec(AUTO_INT_1, saved_int_1);
  SetIntVec(AUTO_INT_5, saved_int_5);

	switch(ID)
	{
		case 2:
		{
			HIDMouse_Kill();
			break;
		}
	}
	
	USB_PeripheralKill();

	//Shut down the driver
	Driver_Kill();

	//Flush the keyboard buffer
	GKeyFlush();
}
