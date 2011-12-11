#include <tigcclib.h>
#include "SerialAdapter.h"
#include "MassStorage.h"
#include "HIDMouse.h"
#include "HIDKeyboard.h"
#include "SilentLink.h"

INT_HANDLER _main_saved_int_1;
INT_HANDLER _main_saved_int_5;

void SaveKeyInterrupts()
{
	//Save AUTO_INT_1 and AUTO_INT_5 interrupts because they interfere with key reading
	_main_saved_int_1 = GetIntVec(AUTO_INT_1);
	_main_saved_int_5 = GetIntVec(AUTO_INT_5);
	SetIntVec(AUTO_INT_1, DUMMY_HANDLER);
	SetIntVec(AUTO_INT_5, DUMMY_HANDLER);
}

void RestoreKeyInterrupts()
{
	//Restore AUTO_INT_1 and AUTO_INT_5 interrupts
  SetIntVec(AUTO_INT_1, _main_saved_int_1);
  SetIntVec(AUTO_INT_5, _main_saved_int_5);
}

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

void DoSerialAdapter(void)
{
	//Display a message to the user
	clrscr();
	printf("Connect a USB cable to\n");
	printf("your calculator now.\n\n");
	printf("Press [CLEAR] to quit.\n\n");
	printf("Install PL2303 USB serial\n");
	printf(" adapter for this device.\n\n");

	//Initialize the driver
	Driver_Initialize();
	SerialAdapter_Initialize(SerialAdapter_HandleReceivingData);

	SaveKeyInterrupts();
	while (!_keytest(RR_CLEAR));
	RestoreKeyInterrupts();

	//Shut down the driver
	SerialAdapter_Kill();
	Driver_Kill();

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
		
	//Shut down the driver
	SilentLink_Kill();
	Driver_Kill();

	//Flush the keyboard buffer
	GKeyFlush();
}

short DoHIDMouse(void)
{
	short result = 0;

	//Display a message to the user
	clrscr();
	printf("Connect a USB cable to\n");
	printf("your calculator now.\n\n");
	printf("Press [CLEAR] to quit.\n");
	printf("Press [APPS] to switch\nto keyboard mode.\n\n");

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
		else if (_keytest(RR_APPS))
		{
			// Switch to keyboard mode.
			result = 1;
			break;
		}
		else
		{
			timer = 0;
		}

		HIDMouse_Do();
	}
	RestoreKeyInterrupts();


	//Shut down the driver
	HIDMouse_Kill();
	Driver_Kill();

	//Flush the keyboard buffer
	GKeyFlush();

	return result;
}

short DoHIDKeyboard()
{
	short result = 0;

	//Display a message to the user
	clrscr();
	printf("Connect a USB cable to\n");
	printf("your calculator now.\n\n");
	printf("Press [ON] to quit.\n");
	printf("Press [APPS] to switch\nto mouse mode.\n\n");

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

		if (HIDKeyboard_Do() != 0) {
			// Switch to mouse mode
			result = 1;
			break;
		}
	}
	RestoreKeyInterrupts();


	//Shut down the driver
	HIDKeyboard_Kill();
	Driver_Kill();

	//Flush the keyboard buffer
	GKeyFlush();

	return result;
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

		//Shut down the driver
		MassStorage_Kill();
		Driver_Kill();

		//Flush the keyboard buffer
		GKeyFlush();
	}
}
