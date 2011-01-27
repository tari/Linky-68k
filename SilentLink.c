#include "SilentLink.h"

unsigned char deviceDescriptor[] = {0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x51, 0x04, 0x04, 0xE0, 0x00, 0x03, 0x01, 0x02, 0x00, 0x01};
unsigned char configDescriptor[] = {0x09, 0x02, 0x1F, 0x00, 0x01, 0x01, 0x00, 0xE0, 0x00, 0x09, 0x04, 0x00, 0x00, 0x02, 0xFF, 0x01, 0x00, 0x00,
                           0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00, 0x07, 0x05, 0x02, 0x02, 0x40, 0x00};

void SilentLink_HandleSetConfiguration()
{
	printf("SetConfiguration callback called!\n");
}

void SilentLink_HandleIncomingData(unsigned char readyMap)
{
	const int MAX_RAW_PACKET_SIZE = 1023;
	unsigned int received;

	if ((readyMap & 0x04) > 0)
	{
		unsigned char buffer[5];
		received = USB_ReceiveBulkData(0x02, buffer, 5);

		if (received == 5)
		{
			//We ignore the first two bytes because we'll never have a raw packet greater than 64KB
			unsigned int rawPacketLength = buffer[3] || (buffer[2] << 8); //|| (buffer[1] << 16) || (buffer[0] << 24);
			unsigned int rawPacketType = buffer[4];
			unsigned char dataBuffer[MAX_RAW_PACKET_SIZE];

			received = USB_ReceiveBulkData(0x02, dataBuffer, rawPacketLength);
			
			if (rawPacketType == 0x01)
			{
				//Return buffer negotiaton response
			}
		}
	}
}

USBPeripheral SilentLink_GetInterface()
{
  USBPeripheral ret = DEFAULT_USB_PERIPHERAL;

  //Set descriptor information
  ret.deviceDescriptor = deviceDescriptor;
  ret.configDescriptor = configDescriptor;

	//Set callbacks
  ret.h_setConfig = SilentLink_HandleSetConfiguration;
  ret.h_incomingData = SilentLink_HandleIncomingData;

  return ret;
}
