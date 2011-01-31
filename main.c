#include <tigcclib.h>
#include "usb.h"
#include "HIDMouse.h"
#include "HIDKeyboard.h"
#include "SilentLink.h"
#include "MassStorage.h"

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
	//Find the "FlppyImg" Flash application and get a pointer to its data
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
 			HANDLE d = DialogNewSimple(140, 40);
			DialogAddXFlags(d, DF_SCREEN_SAVE, XF_ALLOW_VARLINK | XF_VARLINK_SELECT_ONLY, 0, 0, 0);
			DialogAddTitle(d, "Select Mass Storage Image", BT_OK, BT_CANCEL);
			DialogAddRequest(d, 3, 18, "Flash App. Name:", 0, 10, 10);

			int cont = (DialogDo(d, CENTER, CENTER, imageName, NULL) == KEY_ENTER);
			if (cont)
			{
				MassStorage_Initialize(MassStorage_HandleReadSector, NULL);
			}
			HeapFree(d);
			FontSetSys(F_6x8);
			
			if (!cont) return;
			break;
		}
		default:
		{
			return;
		}
	}

	//Display a message to the user
  clrscr();
  printf("Connect a USB cable to\n");
  printf("your calculator now.\n\n");
  printf("Press [ESC] to quit.\n\n");

	//Main key loop
	int timer = 0;
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
					timer++;
					
					if (timer > 10)
					{
						timer = 0;
						HIDMouse_Sensitivity++;
						printf("Changed sensitivity to %02u\n", HIDMouse_Sensitivity);
					}
				}
				else if (_keytest(RR_MINUS))
				{
					timer++;
					
					if (timer > 10)
					{
						timer = 0;
						HIDMouse_Sensitivity--;
						printf("Changed sensitivity to %02u\n", HIDMouse_Sensitivity);
					}
				}
				else
					timer = 0;
				
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
