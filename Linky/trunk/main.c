#include <tigcclib.h>
#include "usb.h"
#include "HIDMouse.h"
#include "HIDKeyboard.h"
#include "SilentLink.h"
#include "MassStorage.h"
#include "SerialAdapter.h"

void DoHostMode(void);
void DoSilentLink(void);
void DoHIDMouse(void);
void DoHIDKeyboard(void);
void DoMassStorage(void);
void DoSerialAdapter(void);

// Definitions from TIFS
typedef HANDLE AppID;

#define MAX_APPLET_NAME_SIZE (8)

typedef struct
{
   unsigned long magic;
   unsigned char name[MAX_APPLET_NAME_SIZE];
   unsigned char zeros[24];
   unsigned short flags;
   unsigned long dataLen;
   unsigned long codeOffset;
   unsigned long initDataOffset;
   unsigned long initDataLen;
   unsigned long optlen;
} AppHdr;

typedef struct SACB
{
   unsigned short flags;
   AppID myID;
   AppID next;
   AppID prev;
   unsigned long publicstorage;
   const AppHdr * appHeader;
   const unsigned char * certhdr;
   pFrame appData;
} ACB;
// End definitions from TIFS

char imageName[11];
unsigned char sectorBuffer[512];

unsigned char* MassStorage_HandleReadSector(unsigned long long int LBA)
{
	//Find the specified Flash application and get a pointer to its data
	HANDLE id = TIOS_EV_getAppID(imageName);
	unsigned char* sptr = NULL;
	unsigned int i;

	if (id != H_NULL)
	{
		unsigned short* ptr = (unsigned short*)(((ACB *)HeapDeref(id))->appHeader);
		do { ptr++; } while (*ptr != 0xC0DE);
		ptr++; sptr = (unsigned char*)ptr;

		//Use the LBA to calculate the address of the sector data
		while (LBA > 0)
		{
			sptr += 512;
			LBA--;
		}

		//Copy it to our buffer
		for (i = 0; i < 512; i++)
			sectorBuffer[i] = sptr[i];
	}
	else
	{
		//Can't find the application, so just assume a buffer of all 0xFFs
		for (i = 0; i < 512; i++)
			sectorBuffer[i] = 0xFF;
	}

	//Return it!
	return sectorBuffer;
}

void SerialAdapter_HandleReceivingData(unsigned int size)
{
	//Receive the data
	unsigned char b[size];
	unsigned int count = SerialAdapter_ReceiveData(b, size);

	//For each byte/character...
	unsigned int i = 0;
	for (i = 0; i < count; i++)
	{
		//Echo it to our LCD
		printf("%c", b[i]);
		
		//Echo it back to the device that sent it to us
		SerialAdapter_SendData(b, 1);
	}
}

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
	DynMenuAdd(m, 13, "Dummy", 15, DMF_TEXT | DMF_CHILD);
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
					DoHIDMouse();
				}
				else if (result == 18)
				{
					DoHIDKeyboard();
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

void DoHostMode(void)
{
	//Display a message to the user
	clrscr();
	printf("Connect a USB cable to\n");
	printf("your calculator now.\n\n");
	printf("Press [CLEAR] to quit.\n\n");

	//Initialize the driver
	Driver_Initialize();
	
	//Main key loop
	SaveKeyInterrupts();
	while (1)
	{
		if (_keytest(RR_CLEAR))
			break;

		if (_keytest(RR_CATALOG))
		{
			while (_keytest(RR_CATALOG));
			
			unsigned char buffer[8];
			USB_GetDescriptor(0x01, buffer, 8);
	
			unsigned int i;
			for (i = 0; i < 8; i++)
				printf("%02X", buffer[i]);
			printf("\n");
		}

		if (_keytest(RR_APPS))
		{
			while (_keytest(RR_APPS));
			
			unsigned char buffer[8];
			USB_GetDescriptor(0x02, buffer, 8);
			
			unsigned int i;
			for (i = 0; i < 8; i++)
				printf("%02X", buffer[i]);
			printf("\n");
		}
	}
	RestoreKeyInterrupts();

	//Shut down the driver
	Driver_Kill();

	// Reinitialize for peripheral mode.
	USB_PeripheralKill();
	*USB_INT_MASK_ADDR = 0xFF;

	//Flush the keyboard buffer
	GKeyFlush();
}

void DoSilentLink(void)
{
	//Display a message to the user
	clrscr();
	printf("Connect a USB cable to\n");
	printf("your calculator now.\n\n");
	printf("Press [CLEAR] to quit.\n\n");

	//Initialize the driver
	Driver_Initialize();
	
	SilentLink_Initialize();

	SaveKeyInterrupts();
	while (!_keytest(RR_CLEAR))
		SilentLink_Do();
	RestoreKeyInterrupts();
		
	SilentLink_Kill();

	//Shut down the driver
	Driver_Kill();

	// Reinitialize for peripheral mode.
	USB_PeripheralKill();
	*USB_INT_MASK_ADDR = 0xFF;

	//Flush the keyboard buffer
	GKeyFlush();
}

void DoSerialAdapter(void)
{
	//Display a message to the user
	clrscr();
	printf("Connect a USB cable to\n");
	printf("your calculator now.\n\n");
	printf("Press [CLEAR] to quit.\n\n");

	//Initialize the driver
	Driver_Initialize();
	
	SerialAdapter_Initialize(SerialAdapter_HandleReceivingData);

	SaveKeyInterrupts();
	while (!_keytest(RR_CLEAR));
	RestoreKeyInterrupts();

	SerialAdapter_Kill();

	//Shut down the driver
	Driver_Kill();

	// Reinitialize for peripheral mode.
	USB_PeripheralKill();
	*USB_INT_MASK_ADDR = 0xFF;

	//Flush the keyboard buffer
	GKeyFlush();
}

void DoHIDMouse(void)
{
	//Display a message to the user
	clrscr();
	printf("Connect a USB cable to\n");
	printf("your calculator now.\n\n");
	printf("Press [CLEAR] to quit.\n\n");

	//Initialize the driver
	Driver_Initialize();
	
	HIDMouse_Initialize();
	
	//Main key loop
	int timer = 0;
	SaveKeyInterrupts();
	while (!_keytest(RR_CLEAR))
	{
		if (_keytest(RR_PLUS))
		{
			timer++;
			
			if (timer > 300)
			{
				timer = 0;
				HIDMouse_Sensitivity++;
				printf("Changed sensitivity to %02u\n", HIDMouse_Sensitivity);
			}
		}
		else if (_keytest(RR_MINUS))
		{
			timer++;
			
			if (timer > 300)
			{
				timer = 0;
				HIDMouse_Sensitivity--;
				printf("Changed sensitivity to %02u\n", HIDMouse_Sensitivity);
			}
		}
		else
			timer = 0;
		
		HIDMouse_Do();
	}
	RestoreKeyInterrupts();

	HIDMouse_Kill();

	//Shut down the driver
	Driver_Kill();

	// Reinitialize for peripheral mode.
	USB_PeripheralKill();
	*USB_INT_MASK_ADDR = 0xFF;

	//Flush the keyboard buffer
	GKeyFlush();
}

void DoHIDKeyboard()
{
	//Display a message to the user
	clrscr();
	printf("Connect a USB cable to\n");
	printf("your calculator now.\n\n");
	printf("Press [ON] to quit.\n\n");

	//Initialize the driver
	Driver_Initialize();

	HIDKeyboard_Initialize();

	//Main key loop
	SaveKeyInterrupts();
	while (1)
	{
		if (!(*((volatile unsigned char *)0x60001A) & 2))
		{
			// ON key pressed, acknowledge interrupt and exit.
			*((volatile unsigned char *)0x60001A) = 0xFF;
			break;
		}
		
		HIDKeyboard_Do();
	}
	RestoreKeyInterrupts();

	HIDKeyboard_Kill();

	//Shut down the driver
	Driver_Kill();

	// Reinitialize for peripheral mode.
	USB_PeripheralKill();
	*USB_INT_MASK_ADDR = 0xFF;

	//Flush the keyboard buffer
	GKeyFlush();
}

void DoMassStorage()
{
	HANDLE d = DialogNewSimple(140, 40);
	DialogAddXFlags(d, DF_SCREEN_SAVE, XF_ALLOW_VARLINK | XF_VARLINK_SELECT_ONLY, 0, 0, 0);
	DialogAddTitle(d, "Select Mass Storage Image", BT_OK, BT_CANCEL);
	DialogAddRequest(d, 3, 18, "Flash App. Name:", 0, 10, 10);

	int cont = (DialogDo(d, CENTER, CENTER, imageName, NULL) == KEY_ENTER);
	HeapFree(d);
	FontSetSys(F_6x8);
	if (cont)
	{
		//Display a message to the user
		clrscr();
		printf("Connect a USB cable to\n");
		printf("your calculator now.\n\n");
		printf("Press [CLEAR] to quit.\n\n");
		
		//Initialize the driver
		Driver_Initialize();

		MassStorage_Initialize(MassStorage_HandleReadSector, NULL);

		SaveKeyInterrupts();
		while (!_keytest(RR_CLEAR))
			MassStorage_Do();
		RestoreKeyInterrupts();
		
		MassStorage_Kill();
		
		//Shut down the driver
		Driver_Kill();

		// Reinitialize for peripheral mode.
		USB_PeripheralKill();
		*USB_INT_MASK_ADDR = 0xFF;

		//Flush the keyboard buffer
		GKeyFlush();
	}
}
