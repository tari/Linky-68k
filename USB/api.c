#include <tigcclib.h>
#include "api.h"

Driver_NewDeviceConnected cb_NewDeviceConnected;

DEFINE_INT_HANDLER(MyInt3)
{
	USB_HandleInterrupt();
}

void Driver_Initialize(void)
{
	//Set default values for everything
	peripheralInterface = NULL;
	cb_NewDeviceConnected = NULL;

  //Back up the old and install the new handler
  OldInt3 = GetIntVec(AUTO_INT_3);
  SetIntVec(AUTO_INT_3, MyInt3);
}

void Driver_Kill(void)
{
	//Restore the old handler
	SetIntVec(AUTO_INT_3, OldInt3);

	USB_PeripheralKill();
}

void Driver_SetPeripheralInterface(USBPeripheral* interface)
{
	//Keep a pointer to this structure
	peripheralInterface = interface;

	//Set up other values
	if (peripheralInterface != NULL)
	{
		bMaxPacketSize0 = peripheralInterface->deviceDescriptor[7];
	}
}

void Driver_SetCallbacks(Driver_NewDeviceConnected newDeviceConnected)
{
	cb_NewDeviceConnected = newDeviceConnected;
}
