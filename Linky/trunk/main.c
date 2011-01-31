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

void _main(void)
{
	short ID = -1;
	//Pick the driver to load
	HANDLE h = PopupNew("Select Driver", 0);
	if (h == H_NULL) return;
	if (PopupAddText(h, -1, "Silent Link", 1) == H_NULL) goto free_dialog;
	if (PopupAddText(h, -1, "HID Mouse", 2) == H_NULL) goto free_dialog;
	if (PopupAddText(h, -1, "HID Keyboard", 3) == H_NULL) goto free_dialog;
	if (PopupAddText(h, -1, "Mass Storage", 4) == H_NULL) goto free_dialog;
	if (PopupAddText(h, -1, "Generic Host", 5) == H_NULL) goto free_dialog;
	ID = PopupDo(h, CENTER, CENTER, 0);
free_dialog:
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
			HeapFree(d);
			FontSetSys(F_6x8);
			if (cont)
			{
				MassStorage_Initialize(MassStorage_HandleReadSector, NULL);
			}
			else
			{
				goto kill_exit;
			}
			break;
		}
		case 5:
		{
			//Do nothing
			break;
		}
		default:
		{
			goto kill_exit;
		}
	}

	//Display a message to the user
	clrscr();
	printf("Connect a USB cable to\n");
	printf("your calculator now.\n\n");
	printf("Press [ON] to quit.\n\n");

	//Main key loop
	int timer = 0;
	while (1)
	{
		if (!(*((volatile unsigned char *)0x60001A) & 2))
		{
			// ON key pressed, acknowledge interrupt and exit.
			*((volatile unsigned char *)0x60001A) = 0xFF;
			break;
		}

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
			case 5:
			{
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

kill_exit:
	//Shut down the driver
	Driver_Kill();

	//Flush the keyboard buffer
	GKeyFlush();
}
