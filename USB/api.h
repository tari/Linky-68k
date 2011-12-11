#ifndef _API_H
#define _API_H

#include <tigcclib.h>
#include "usb.h"

typedef struct
{
	unsigned int bFunctionAddress;
	unsigned int bParentAddress;
	unsigned int bHubPortNumber;
} USBDevice;

//Callbacks
typedef void (*Driver_NewDeviceConnected)(USBDevice* device);

//Basic driver functions
void Driver_Initialize();
void Driver_Kill();
void Driver_SetPeripheralInterface(USBPeripheral* interface);
void Driver_SetCallbacks(Driver_NewDeviceConnected newDeviceConnected);

#endif
