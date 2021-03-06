#include <tigcclib.h>
#include "api.h"

extern Driver_NewDeviceConnected cb_NewDeviceConnected;

void USB_HandleControlPacket(unsigned char* packet)
{
	printf("Handling control request\n");
	unsigned char bmRequestType = packet[0] & 0xFF;
	unsigned char bRequest = packet[1];
	unsigned int wValue = (packet[3] << 8) | (packet[2] & 0xFF);
	unsigned int wIndex = (packet[5] << 8) | (packet[4] & 0xFF);
	unsigned int wLength = (packet[7] << 8) | (packet[6] & 0xFF);
	int handled = 0;

	switch(bmRequestType)
	{
		//Other requests
		case 0xA3:
		case 0x23:
		{
			switch(bRequest)
			{
				case 0x03: //hub set feature
				{
					USB_FinishControlRequest();
					if (peripheralInterface->h_hubSetFeature != NULL)
						peripheralInterface->h_hubSetFeature(wValue, wIndex);
					handled = 1;
					break;
				}
				case 0x01: //hub clear feature
				{
					USB_FinishControlRequest();
					if (peripheralInterface->h_hubClearFeature != NULL)
						peripheralInterface->h_hubClearFeature(wValue, wIndex);
					handled = 1;
					break;
				}
				case 0x00: //hub get status
				{
					unsigned char data[4] = {0, 0, 0, 0};
					if (peripheralInterface->h_hubGetStatus != NULL)
						peripheralInterface->h_hubGetStatus(data, wIndex);

					USB_StartControlOutput(data, 4);
					handled = 1;
					break;
				}
			}
			break;
		}
		//Device requests
		case 0x00:
		{
			switch(bRequest)
			{
				case 0x05: //set address
				{
					USB_FinishControlRequest();

					wAddress = (wValue & 0xFF);
					newAddressReceived = 1;

					if (peripheralInterface->h_setAddress != NULL)
						peripheralInterface->h_setAddress(wAddress);

					handled = 1;
					break;
				}
				case 0x09: //set configuration
				{
					if (peripheralInterface->h_setConfig != NULL)
						peripheralInterface->h_setConfig();
					USB_FinishControlRequest();
					
					//HACK: Make sure everything is enabled for input
					*USB_DATA_IN_EN_ADDR1 = 0x0E;

					handled = 1;
				}
			}

			break;
		}
		case 0x80:
		{
			switch(bRequest)
			{
				case 0x06: //get descriptor
				{
					unsigned int descType = (wValue >> 8);
					unsigned int descLength = 0;

					const unsigned char* address = NULL;

					if (peripheralInterface != NULL)
					{
						if (descType == 0x01)
						{
							address = peripheralInterface->deviceDescriptor;
							descLength = peripheralInterface->deviceDescriptor[0];
						}
						else if(descType == 0x02)
						{
							address = peripheralInterface->configDescriptor;
							descLength = (peripheralInterface->configDescriptor[2] & 0xFF) | (peripheralInterface->configDescriptor[3] << 8);
						}
						else
							break;
					}
					else
						break;

					int count = (descLength > wLength)? wLength : descLength;
					USB_StartControlOutput(address, count);
					handled = 1;
				}
			}
			break;
		}
		//Interface requests
		case 0x01:
		{
			switch(bRequest)
			{
				case 0x0B: //set interface
				{
					USB_FinishControlRequest();
					handled = 1;
					break;
				}
			}

			break;
		}
		//Endpoint requests
		case 0x02:
		{
			switch(bRequest)
			{
				case 0x01: //clear feature
				{
					volatile unsigned char endpoint = *USB_SELECTED_ENDPOINT_ADDR;
					volatile unsigned char value;

					*USB_SELECTED_ENDPOINT_ADDR = (unsigned char)(wIndex & 0x7F);
					if ((wIndex & 0x80))
					{
						//Incoming endpoint, clear the halt condition
						value = *USB_OUTGOING_CMD_ADDR;
						value &= 0xEF;
						value |= 0x40;
						*USB_OUTGOING_CMD_ADDR = value;
					}
					else
					{
						//Outgoing endpoint, clear the halt condition
						value = *USB_INCOMING_CMD_ADDR;
						value &= 0xDF;
						value |= 0x80;
						*USB_INCOMING_CMD_ADDR = value;
					}
					
					*USB_SELECTED_ENDPOINT_ADDR = endpoint;
					
					USB_FinishControlRequest();
					handled = 1;
					break;
				}
			}
		}
	}

	//Did we manage to handle this request?
	if (handled <= 0)
	{
		//No, so try and pass it on out
		if (peripheralInterface->h_unknownControlRequest != NULL)
			handled = peripheralInterface->h_unknownControlRequest(bmRequestType, bRequest, wValue, wIndex, wLength);

		//Now have we handled it?
		if (handled <= 0)
		{
			//No, and since we still don't know how to handle it, just assume it's a request with no data stage
			USB_FinishControlRequest();
		}
	}
}

void USB_HandleInterrupt(void)
{
	unsigned char status1 = *USB_INT_STATUS1_ADDR;
	unsigned char status2 = *USB_INT_STATUS2_ADDR;

	if ((status1 & 0x04) == 0)
	{
		//Need to check offset 0x56 for what happened

		if ((status2 & 0x40) != 0)
		{
			//Handle USB B-cable connect
			USBState = 0;
			newAddressReceived = 0;
			printf("Periph connect detected\n");

			if (peripheralInterface->h_connected != NULL)
				peripheralInterface->h_connected();

			USB_PeripheralInitialize();
			OSTimerRestart(2);
		}
		else if ((status2 & 0x80) != 0)
		{
			//Handle USB B-cable disconnect
			printf("Periph kill detected\n");
			USB_PeripheralKill();
			OSTimerRestart(2);
		}
		else if ((status2 & 0x10) != 0)
		{
			//Handle USB A-cable connect
			*USB_INT_MASK_ADDR = *USB_INT_MASK_ADDR & (~0x10 & 0xFF);
			*USB_INT_MASK_ADDR |= 0x10;

			//Raise any event we might have for this
			if (cb_NewDeviceConnected != NULL)
				cb_NewDeviceConnected(NULL);

			//Reset the timer
			OSTimerRestart(2);
		}
		else if ((status2 & 0x20) != 0)
		{
			//Handle USB A-cable disconnect
			printf("A cable disconnected\n");
			USB_HostKill();
			printf("A cable disconn handle done\n");
			OSTimerRestart(2);
		}
		else if ((status2 & 0x02) != 0)
		{
			printf("Bit 1 of 0x56 set, host initting\n");
			*USB_INT_MASK_ADDR = *USB_INT_MASK_ADDR & (~0x02 & 0xFF);
			*USB_INT_MASK_ADDR |= 0x02;
			OSTimerRestart(2);
		}
		else if ((status2 & 0x08) != 0)
		{
			printf("Bit 3 of port 0x56 set\n");
			*USB_INT_MASK_ADDR = *USB_INT_MASK_ADDR & (~0x08 & 0xFF);
			*USB_INT_MASK_ADDR |= 0x08;
			OSTimerRestart(2);
		}
		else
		{
			//Unknown 0x56 event
			//Let the AMS deal with it
			printf("Unknown 0x56 event fired: %02X\n", status2);
			ExecuteHandler(OldInt3);
		}
	}
	else if ((status1 & 0x10) == 0)
	{
		//Data waiting (or something else involving data)
		unsigned char outgoingDataSuccess = *USB_OUTGOING_DATA_SUCCESS_ADDR & 0xFF;
		unsigned char incomingDataReady = *USB_INCOMING_DATA_READY_ADDR & 0xFF;
		unsigned char dataStatus = *USB_DATA_STATUS_ADDR & 0xFF;
		unsigned char mode = *USB_MODE_ADDR & 0xFF;

		if (incomingDataReady & 0x0F)
		{
			printf("Incoming data ready\n");
			//Clear the buffered byte count for each endpoint we have data for
			int i;
			unsigned char readyMap = incomingDataReady;
			for (i = 0; i < 8; i++)
			{
				if (readyMap & 0x01)
					bytesBuffered[i] = 0x00;
				readyMap = (readyMap >> 1);
			}
			incomingDataReadyMap = incomingDataReady;

			//Handle incoming bulk/interrupt data
			if (peripheralInterface->h_incomingData != NULL)
				peripheralInterface->h_incomingData(incomingDataReady);
		}
		else
		{
			if (mode & 0x04)
			{
				//Calculator is in host mode, handle data waiting accordingly
				printf("Incoming: 82 %02X, 84 %02X, 86 %02X\n", outgoingDataSuccess, incomingDataReady, dataStatus);
				OSTimerRestart(2);
			}
			else
			{
				//Calculator is in peripheral mode, handle data waiting accordingly
				if (dataStatus & 0x04)
				{
					//Error indicates control pipe is not enabled for output, so enable it
					*USB_INT_ENABLE_ADDR = (char)1;
					*USB_DATA_OUT_EN_ADDR1 |= (char)1;
				}
				else if (outgoingDataSuccess & 0x01)
				{
					*USB_SELECTED_ENDPOINT_ADDR = 0x00;
					char outgoingValue = *USB_OUTGOING_CMD_ADDR;

					if (!(outgoingValue & 0x04))
					{
						if (outgoingValue & 0x10)
						{
							//I'm not exactly sure what this is...
							*USB_OUTGOING_CMD_ADDR = (outgoingValue | 0x80);
						}

						//If we're not busy doing something else...
						if (USBState == 0x00)
						{
							if (outgoingValue & 0x01)
							{
								//Receive and handle this control packet
								unsigned char controlPacket[8];
								unsigned int i;

								for (i = 0; i < 8; i++)
									controlPacket[i] = (*USB_ENDPOINT0_DATA_ADDR & 0xFF);

								USB_HandleControlPacket(controlPacket);
							}
							else
							{
								if (newAddressReceived > 0)
								{
									//So apparently now we set the function address
									newAddressReceived = 0;
									USB_SetFunctionAddress(wAddress);
								}
							}
						}

						//See if we need to send more data for a control request's response
						if (USBState == 1)
						{
							//We do, so get to it
							unsigned int count = (responseBytesRemaining > bMaxPacketSize0)? bMaxPacketSize0 : responseBytesRemaining;
							unsigned int i;

							//Buffer the data
							unsigned char dataBuffer[count];
							for (i = 0; i < count; i++)
								dataBuffer[i] = controlDataAddress[i];

							//Send the data
							for (i = 0; i < count; i++)
							{
								*USB_ENDPOINT0_DATA_ADDR = dataBuffer[i];
							}
							controlDataAddress += count;
							responseBytesRemaining -= count;
	
							if (responseBytesRemaining > 0)
							{
								//If there's still data to send, send the continuation command
								*USB_SELECTED_ENDPOINT_ADDR = (char)0x00;
								*USB_OUTGOING_CMD_ADDR |= (char)0x02;
							}
							else
							{
								USB_FinishControlOutput();
								USBState = 0;
							}
						}
					}
					else
					{
						//I'm not exactly sure what this is...
						*USB_OUTGOING_CMD_ADDR = (outgoingValue & 0xFB);
					}
				}
			}
		}

		OSTimerRestart(2);
	}
	else
	{
		//Unknown USB event
		//Let the AMS deal with it
		ExecuteHandler(OldInt3);
	}
}
