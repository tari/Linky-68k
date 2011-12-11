#ifndef _API_H
#define _API_H

#include <tigcclib.h>
#include "usb.h"

//Basic driver functions
void Driver_Initialize();
void Driver_Kill();
void Driver_SetPeripheralInterface(USBPeripheral* interface);

#endif
