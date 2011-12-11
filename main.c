#include <tigcclib.h>
#include "api.h"
#include "demos.h"

INT_HANDLER main_saved_int_1;
INT_HANDLER main_saved_int_5;

void SaveKeyInterrupts()
{
	//Save AUTO_INT_1 and AUTO_INT_5 interrupts because they interfere with key reading
	main_saved_int_1 = GetIntVec(AUTO_INT_1);
	main_saved_int_5 = GetIntVec(AUTO_INT_5);
	SetIntVec(AUTO_INT_1, DUMMY_HANDLER);
	SetIntVec(AUTO_INT_5, DUMMY_HANDLER);
}

void RestoreKeyInterrupts()
{
	//Restore AUTO_INT_1 and AUTO_INT_5 interrupts
  SetIntVec(AUTO_INT_1, main_saved_int_1);
  SetIntVec(AUTO_INT_5, main_saved_int_5);
}

void NewDeviceConnected(USBDevice* device)
{
	printf("New device connected!\n");
	USB_HostInitialize();
}

void DoHostMode(void)
{
	const int BUFFER_SIZE = 256;

	//Display a message to the user
	clrscr();
	printf("Connect a mini-A USB\n");
	printf("host cable to your\n");
	printf("calculator now.\n\n");
	printf("Press [CLEAR] to quit.\n\n");

	//Initialize the driver
	Driver_Initialize();
	Driver_SetCallbacks(NewDeviceConnected);

	//Main key loop
	SaveKeyInterrupts();
	while (1)
	{
		if (_keytest(RR_CLEAR))
			break;

		if (_keytest(RR_CATALOG))
		{
			while (_keytest(RR_CATALOG)); //debounce
			
			//Get device descriptor
			unsigned char buffer[BUFFER_SIZE];
			USB_GetDeviceDescriptor(buffer, BUFFER_SIZE);

			//Show first 8 bytes
			unsigned int i;
			for (i = 0; i < 8; i++)
				printf("%02X", buffer[i]);
			printf("\n");
		}

		if (_keytest(RR_APPS))
		{
			while (_keytest(RR_APPS)); //debounce
			
			//Get configuration descriptor
			unsigned char buffer[BUFFER_SIZE];
			USB_GetConfigurationDescriptor(buffer, BUFFER_SIZE);
			
			//Show first 8 bytes
			unsigned int i;
			for (i = 0; i < 8; i++)
				printf("%02X", buffer[i]);
			printf("\n");
		}
	}
	RestoreKeyInterrupts();

	//Shut down the driver
	Driver_Kill();

	//Flush the keyboard buffer
	GKeyFlush();
}

void _main(void)
{
	//Start the main menu
	clrscr();
	HANDLE m = MenuNew(MF_TOOLBOX, 160, 0);
	if (m == H_NULL) return;
	DynMenuAdd(m, 0, "Tools", 1, DMF_TEXT | DMF_TOP_SUB);
	DynMenuAdd(m, 1, "Descriptors", 5, DMF_TEXT | DMF_CHILD_SUB);
	DynMenuAdd(m, 5, "Device", 6, DMF_TEXT | DMF_CHILD);
	DynMenuAdd(m, 5, "Configuration", 7, DMF_TEXT | DMF_CHILD);
	DynMenuAdd(m, 5, "String", 8, DMF_TEXT | DMF_CHILD);
	DynMenuAdd(m, 1, "Logging", 9, DMF_TEXT | DMF_CHILD_SUB);
	DynMenuAdd(m, 9, "Log Viewer", 10, DMF_TEXT | DMF_CHILD);
	DynMenuAdd(m, 9, "Start Log", 11, DMF_TEXT | DMF_CHILD);
	DynMenuAdd(m, 9, "Stop Log", 12, DMF_TEXT | DMF_CHILD);
	DynMenuAdd(m, 0, "Demos", 2, DMF_TEXT | DMF_TOP_SUB);
	DynMenuAdd(m, 2, "Host", 13, DMF_TEXT | DMF_CHILD_SUB);
	DynMenuAdd(m, 2, "Peripheral", 14, DMF_TEXT | DMF_CHILD_SUB);
	DynMenuAdd(m, 13, "Host Service", 15, DMF_TEXT | DMF_CHILD);
	DynMenuAdd(m, 14, "Silent Link", 16, DMF_TEXT | DMF_CHILD);
	DynMenuAdd(m, 14, "HID Mouse", 17, DMF_TEXT | DMF_CHILD);
	DynMenuAdd(m, 14, "HID Keyboard", 18, DMF_TEXT | DMF_CHILD);
	DynMenuAdd(m, 14, "Mass Storage", 19, DMF_TEXT | DMF_CHILD);
	DynMenuAdd(m, 14, "PL2303 Serial Adapter", 20, DMF_TEXT | DMF_CHILD);
	DynMenuAdd(m, 0, "About", 3, DMF_TEXT | DMF_TOP);
	DynMenuAdd(m, 0, "Quit", 4, DMF_TEXT | DMF_TOP);
	HANDLE e = MenuBegin(NULL, 0, 0, MBF_HMENU | MBF_MAX_MENU_WIDTH, 160, m);
	short result, key;

	do
	{
		key = ngetchx();

		if (key == KEY_ESC)
		{
			break;
		}
		else
		{
			result = MenuKey(e, key);
			if (result == 4)
				break;
			
			if (result != M_NOTMENUKEY)
			{
				MenuOff(e);

				if (result == 15)
				{
					DoHostMode();
				}
				else if (result == 16)
				{
					DoSilentLink();
				}
				else if (result == 17)
				{
doHIDMouse:
					if (DoHIDMouse() != 0) {
						goto doHIDKeyboard;
					}
				}
				else if (result == 18)
				{
doHIDKeyboard:
					if (DoHIDKeyboard() != 0) {
						goto doHIDMouse;
					}
				}
				else if (result == 19)
				{
					DoMassStorage();
				}
				else if (result == 20)
				{
					DoSerialAdapter();
				}
				else if (result == 3)
				{
					//Draw about dialog
					HANDLE h = DialogNewSimple(150, 50);
					DialogAddTitle(h, "Linky", BT_OK, BT_NONE);
					DialogAddText(h, 3, 15, "Brandon Wilson");
					DialogAddText(h, 3, 21, "Lionel Debroux");
					DialogAddText(h, 3, 30, "http://brandonw.net/svn/calcstuff/Linky");
					DialogDo(h, CENTER, CENTER, NULL, NULL);
					HeapFree(h);
				}

				clrscr();
				MenuOn(e);
			}
		}

		ST_busy(ST_NORMAL);
	} while (1);
	MenuEnd(e);
	MenuUpdate();
}
