#include <tigcclib.h>
#include "usb.h"

unsigned char HIDMouse_Sensitivity;

void HIDMouse_Initialize(void);
void HIDMouse_Kill(void);
void HIDMouse_Do(void);
