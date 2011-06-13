#include <tigcclib.h>
#include "usb.h"

typedef unsigned char* (*Handle_DataReceived)(unsigned int size);

void SerialAdapter_Initialize(Handle_DataReceived dataReceived);
void SerialAdapter_Kill(void);
void SerialAdapter_Do(void);
