#include <tigcclib.h>
#include "usb.h"

USBPeripheral* peripheralInterface;         //Pointer to USB peripheral mode setup/interface information
INT_HANDLER OldInt3;                        //Backup of old AUTO_INT_3 interrupt vector
unsigned char bMaxPacketSize0;              //used in control pipe communication
const unsigned char* controlDataAddress;    //used in sending back control request responses
unsigned int responseBytesRemaining;        //used in sending back control request responses
int USBState;                               //used in sending back control request responses
int newAddressReceived;                     //used in setting the address from the interrupt
int wAddress;                               //used in setting the address from the interrupt
int bytesBuffered[0x0F];                    //keeps track of buffered incoming data per pipe
unsigned char incomingDataReadyMap;         //keeps track of incoming data per pipe

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
			printf("Host init returned: %02X\n", USB_HostInitialize());
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
			*USB_INT_MASK_ADDR = *USB_INT_MASK_ADDR & 0xFD;
			*USB_INT_MASK_ADDR |= *USB_INT_MASK_ADDR & 0x02;
			OSTimerRestart(2);
		}
		else if ((status2 & 0x08) != 0)
		{
			printf("Bit 3 of port 0x56 set\n");
			*USB_INT_MASK_ADDR = *USB_INT_MASK_ADDR & 0xF7;
			*USB_INT_MASK_ADDR |= *USB_INT_MASK_ADDR & 0x08;
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

int USB_IsDataReady(unsigned char endpoint)
{
	return (incomingDataReadyMap & (endpoint << 1)) ? 1 : 0;
}

void USB_IndicateNotReady(unsigned char endpoint)
{
	incomingDataReadyMap &= ~(endpoint << 1);
}

void USB_KillPower(void)
{
	//Set bit 1, which cuts power to the USB controller
	*USB_BASE_POWER_ADDR = (unsigned char)2;

	//Enable all USB-related interrupts
	*USB_INT_ENABLE_ADDR = (char)1;
	*USB_INT_MASK_ADDR = (char)0xFF;
}

//HACK: I'm pretty sure this routine is completely useless, but I could be wrong
unsigned char USB_OutgoingCmdDummyRead(void)
{
	volatile unsigned char ret = *USB_OUTGOING_CMD_ADDR;

	return ret;
}

void USB_FinishControlRequest(void)
{
	*USB_SELECTED_ENDPOINT_ADDR = (unsigned char)0x00;
	USB_OutgoingCmdDummyRead();
	
	*USB_OUTGOING_CMD_ADDR = (unsigned char)0x48;
	USB_OutgoingCmdDummyRead();
}

void USB_FinishControlOutput(void)
{
	*USB_SELECTED_ENDPOINT_ADDR = (unsigned char)0x00;
	*USB_OUTGOING_CMD_ADDR |= (unsigned char)0x0A;

	//WHY DOESN'T THIS WORK?! THE 84+/SE DO IT!
	//_WaitOutgoingCmdSuccess();
	//_WaitOutgoingCmdSuccess();

	if (peripheralInterface->h_controlOutputDone != NULL)
		peripheralInterface->h_controlOutputDone();
}

void USB_SetFunctionAddress(int address)
{
	*USB_FUNCTION_ADDRESS_ADDR = (unsigned char)(address & 0xFF);
}

//HACK: This doesn't take into account max packet size for the specified endpoint...
int USB_SendInterruptData(unsigned char endpoint, unsigned char* data, unsigned int count)
{
	//Back up current endpoint
	unsigned char previousEndpoint = *USB_SELECTED_ENDPOINT_ADDR;
	int cmdValue;

	//Set endpoint
	*USB_SELECTED_ENDPOINT_ADDR = (endpoint & 0x7F);

	//Loop until 1,(91h) is zero
	unsigned long timeout = 0xFFFF;
	do
	{
		cmdValue = *USB_OUTGOING_CMD_ADDR & 0x01;
		if (--timeout == 0)
			return -1; //crap...
	} while (cmdValue);

	//Send bytes
	unsigned int i;
	for( i = 0; i < count; i++ )
	{
		*(USB_ENDPOINT0_DATA_ADDR+endpoint) = data[i];
	}

	//Set endpoint
	*USB_SELECTED_ENDPOINT_ADDR = (endpoint & 0x7F);

	//Set 1,(91h)
	*USB_OUTGOING_CMD_ADDR = 0x01;

	//Restore endpoint
	*USB_SELECTED_ENDPOINT_ADDR = previousEndpoint;
	
	return 0;
}

int USB_ReceiveInterruptData(unsigned char endpoint, unsigned char* data, unsigned int count)
{
	int ret = 0;

	//Set endpoint
	*USB_SELECTED_ENDPOINT_ADDR = (endpoint & 0x7F);
	
	//Read count (discarding it)
	unsigned char bytesInQueue = *USB_INCOMING_DATA_COUNT_ADDR;

	//Read data
	unsigned int i;
	volatile unsigned char* dataPipe = USB_ENDPOINT0_DATA_ADDR + endpoint;
	for( i = 0; i < count; i++ )
	{
		data[i] = *dataPipe;
	}

	bytesInQueue -= count;

	*USB_INCOMING_CMD_ADDR &= (unsigned char)0xFE;
	*USB_INIT_RELATED1_ADDR = (unsigned char)0x21;
	
	return ret;
}

void USB_SendBulkData(unsigned char endpoint, unsigned char* data, unsigned int count)
{
	unsigned char* dataBuffer = data;
	unsigned int bytesToSend;

	//Set endpoint
	*USB_SELECTED_ENDPOINT_ADDR = (endpoint & 0x7F);

	//HACK: We should do this somewhere else, this assumes bulk endpoint with max packet size 0x40
	*USB_OUTGOING_PIPE_SETUP_ADDR = (endpoint & 0x7F) | 0x20;
	*USB_OUTGOING_MAX_PACKET_SIZE_ADDR = 0x08;
	*USB_DATA_OUT_EN_ADDR1 = 0xFF;
	*USB_UNKNOWN_92_ADDR = 0x00;
	*USB_INIT_RELATED1_ADDR = 0xA1;
	
	//Disable interrupts
	*USB_INT_ENABLE_ADDR = (unsigned char)0x00;
	
	while (count > 0)
	{
		bytesToSend = count > 0x40? 0x40 : count;
		
		//Send data
		unsigned int i;
		volatile unsigned char* dataPipe = USB_ENDPOINT0_DATA_ADDR + (endpoint & 0x7F);
		for (i = 0; i < bytesToSend; i++)
		{
			*dataPipe = dataBuffer[i];
		}

		dataBuffer += bytesToSend;
		count -= bytesToSend;

		//Send continuation commands
		volatile unsigned char val;
		*USB_OUTGOING_CMD_ADDR = (unsigned char)0x01;
		do
		{
			val = *USB_OUTGOING_DATA_SUCCESS_ADDR;
		} while ((val & (1 << endpoint)) == 0);
		*USB_OUTGOING_CMD_ADDR = (unsigned char)0x08;
	}
	
	//Re-enable USB interrupts
	*USB_INT_ENABLE_ADDR = (unsigned char)0x01;
}

//HACK: This currently can't receive more than the max packet size at one time
int USB_ReceiveBulkData(unsigned char endpoint, unsigned char* data, unsigned int count)
{
	int ret = 0;

	//Set endpoint
	*USB_SELECTED_ENDPOINT_ADDR = (endpoint & 0x7F);

	//Have we NAK'd/Stalled?
	int NAKStall = *USB_INCOMING_CMD_ADDR & 0x7F;
	if (NAKStall & 0x40)
	{
		//Clear the NAK/Stall condition?
		*USB_INCOMING_CMD_ADDR = (NAKStall & 0xDF);
	}
	else
	{
		//Do we have any buffered bytes?
		int bytesInQueue = bytesBuffered[endpoint & 0x0F];
		if (bytesInQueue <= 0x00)
		{
			//No, so read count
			bytesInQueue = *USB_INCOMING_DATA_COUNT_ADDR;
		}

		//Read data
		unsigned int i;
		volatile unsigned char* dataPipe = USB_ENDPOINT0_DATA_ADDR + (endpoint & 0x7F);
		for( i = 0; i < count; i++ )
		{
			data[i] = *dataPipe;
			ret++;
		}

		if (bytesInQueue > 0)
			bytesInQueue -= ret;
		bytesBuffered[endpoint & 0x0F] = bytesInQueue;

		if (bytesInQueue <= 0)
		{
			*USB_INCOMING_CMD_ADDR &= (unsigned char)0xFE;
			*USB_INIT_RELATED1_ADDR = (unsigned char)0xA1;
			
			USB_IndicateNotReady(endpoint);
		}
	}
	
	return ret;
}

void USB_StartControlOutput(const unsigned char* address, int bytesRemaining)
{
	controlDataAddress = address;
	responseBytesRemaining = bytesRemaining;
	USBState = 1;

	*USB_SELECTED_ENDPOINT_ADDR = (unsigned char)0x00;
	*USB_OUTGOING_CMD_ADDR = (unsigned char)0x40;
}

//IS THIS EVEN BEING USED?!
void USB_SendControlData(unsigned char* data, unsigned int length)
{
	unsigned int sent = 0;
	unsigned int i;

	while (sent < length)
	{
		unsigned int left = length - sent;
		unsigned int count = (left > bMaxPacketSize0)? bMaxPacketSize0 : left;

		//Send the data
		for (i = 0; i < count; i++)
			*USB_ENDPOINT0_DATA_ADDR = data[i+sent];
			
		sent += count;

		if (sent < length)
		{
			//If there's still data to send, send the continuation command
			*USB_SELECTED_ENDPOINT_ADDR = (unsigned char)0x00;
			*USB_OUTGOING_CMD_ADDR = (unsigned char)0x02;
			
			//Why are we not waiting on port 0x82 here?
		}
	}
}

unsigned char USB_WaitOutgoingCmdSuccess()
{
	const unsigned int timeout = 0xFFFF;
	unsigned int i;

	for (i = 0; i < timeout; i++)
	{
		if (*USB_OUTGOING_DATA_SUCCESS_ADDR)
			break;
	}
	
	return i >= timeout ? -1 : 0;
}

void USB_ReceiveControlData(unsigned char* data, unsigned int length)
{
	unsigned int received = 0;
	unsigned int i;

	//Start the data stage
	*USB_SELECTED_ENDPOINT_ADDR = (unsigned char)0x00;
	*USB_OUTGOING_CMD_ADDR = (unsigned char)0x40;
	USB_WaitOutgoingCmdSuccess();
	
	while (received < length)
	{
		unsigned int left = length - received;
		unsigned count = (left > bMaxPacketSize0)? bMaxPacketSize0 : left;
		
		//Receive the data to our buffer
		for (i = 0; i < count; i++)
			data[i+received] = *USB_ENDPOINT0_DATA_ADDR;
		
		received += count;
		
		if (received < length)
		{
			//If there's still data to receive, send the continuation command
			*USB_SELECTED_ENDPOINT_ADDR = (unsigned char)0x00;
			*USB_OUTGOING_CMD_ADDR = (unsigned char)0x02;

			//Not sure if this is necessary or not...(or if it even works!)
			USB_WaitOutgoingCmdSuccess();
		}
	}
}

void USB_SetupOutgoingPipe(unsigned char endpoint, USB_EndpointType type, unsigned char maxPacketSize)
{
	*USB_SELECTED_ENDPOINT_ADDR = endpoint;

	//Set it up!
	*USB_OUTGOING_PIPE_SETUP_ADDR = (endpoint & 0x7F) | type;
	*USB_OUTGOING_MAX_PACKET_SIZE_ADDR = maxPacketSize / 8;

	//I'm not sure what these are just yet
	*USB_DATA_OUT_EN_ADDR1 = (unsigned char)0xFF;
	*USB_UNKNOWN_92_ADDR = (unsigned char)0x00;
	*USB_INIT_RELATED1_ADDR = (unsigned char)0xA1;
}

unsigned char USB_SendControlCmd(unsigned char cmd)
{
	*USB_SELECTED_ENDPOINT_ADDR = (unsigned char)0x00;
	*USB_OUTGOING_CMD_ADDR = cmd;
	
	USB_WaitOutgoingCmdSuccess();
	
	return *USB_OUTGOING_CMD_ADDR & 0x14;
}

unsigned char USB_GetMaxPacketSize0(void)
{
	//Buffer the request data
	const unsigned char request[] = {0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x08, 0x00};
	unsigned char response[8];
	unsigned int i;
	unsigned char status;
	for (i = 0; i < sizeof(request); i++)
		*USB_ENDPOINT0_DATA_ADDR = request[i];
		
	//Send off the request
	status = USB_SendControlCmd(0x0A);
	
	//Send an IN token?
	status = USB_SendControlCmd(0x20);
	
	//Get the response
	for (i = 0; i < sizeof(response); i++)
		response[i] = *USB_ENDPOINT0_DATA_ADDR;

	//Finish it all off
	status = USB_SendControlCmd(0x42);

	//Return bMaxPacketSize0
	return response[7];
}

unsigned char USB_SendControlRequest(unsigned int bMaxPacketSize0, unsigned char* request, unsigned char* responseBuffer, unsigned char bytesExpectedToReceive)
{
	unsigned char* responseAddress = responseBuffer;
	unsigned int i;
	unsigned char status;
	
	//Buffer the request data
	for (i = 0; i < 8; i++)
		*USB_ENDPOINT0_DATA_ADDR = request[i];
	
	//Send off the request
	status = USB_SendControlCmd(0x0A);
	
	if (bytesExpectedToReceive == 0)
	{
		//Just go to the status stage
		status = USB_SendControlCmd(0x60);
	}
	else
	{
		//Receive all the data first
		unsigned int count = 0;

		while (bytesExpectedToReceive > 0)
		{
			//Request IN data
			USB_SendControlCmd(0x20);
			
			//Receive it
			count = bytesExpectedToReceive > bMaxPacketSize0 ? bMaxPacketSize0 : bytesExpectedToReceive; //HACK: assuming bMaxPacketSize0 is 0x40
			for (i = 0; i < count; i++)
				responseAddress[i] = *USB_ENDPOINT0_DATA_ADDR;
			
			responseAddress += count;
			bytesExpectedToReceive -= count;
		}
		
		//Now go to the status stage
		status = USB_SendControlCmd(0x42);
	}
	
	return 0;
}

unsigned char USB_GetDeviceDescriptor(unsigned char* responseBuffer, unsigned int bufferLength)
{
	const unsigned int BUFFER_SIZE = 8;
	unsigned char buffer[BUFFER_SIZE];

	USB_GetDescriptor(0x01, buffer, BUFFER_SIZE);
	USB_GetDescriptor(0x01, responseBuffer, bufferLength >= buffer[0] ? buffer[0] : bufferLength);

	return 0;
}

unsigned char USB_GetConfigurationDescriptor(unsigned char* responseBuffer, unsigned int bufferLength)
{
	const unsigned int BUFFER_SIZE = 8;
	unsigned char buffer[BUFFER_SIZE];
	
	USB_GetDescriptor(0x02, buffer, BUFFER_SIZE);
	unsigned int size = (buffer[2] & 0xFF) | (buffer[3] << 8);
	USB_GetDescriptor(0x02, responseBuffer, bufferLength >= size ? size : bufferLength);
	
	return 0;
}

unsigned char USB_GetDescriptor(unsigned char type, unsigned char* responseBuffer, unsigned int bytesExpectedToReceive)
{
	unsigned char request[] = {0x80, 0x06, 0x00, type, 0x00, 0x00, (bytesExpectedToReceive & 0xFF), (bytesExpectedToReceive >> 8)};
	
	USB_SendControlRequest(0x40, request, responseBuffer, bytesExpectedToReceive);
	
	return 0;
}

unsigned char USB_SetHostAddress(unsigned char address)
{
	//Buffer the request data
	unsigned char request[] = {0x00, 0x05, address, 0x00, 0x00, 0x00, 0x00, 0x00};
	
	USB_SendControlRequest(0x40, request, NULL, 0);
	USB_SetFunctionAddress(address);
	
	return 0;
}

DEFINE_INT_HANDLER(MyTimerHandler)
{
	FiftyMsecTick++;
}

int USB_HostInitialize(void)
{
	INT_HANDLER AutoInt5Backup = GetIntVec(AUTO_INT_5);
	SetIntVec(AUTO_INT_5, MyTimerHandler);

	volatile unsigned char val;
	unsigned long i = 0;
	volatile unsigned long timerValue;
	int ret = 0;

	val = *USB_INIT_4C_ADDR;
	if (val == 0x1A || val == 0x5A)
	{
		//Already initialized
	}
	else
	{
		val = *USB_INIT_4D_ADDR;
		if (val & 0x20)
			*USB_INT_MASK_ADDR = 0x10;
		
		*USB_INIT_4C_ADDR = (unsigned char)0x00;
		*USB_INT_ENABLE_ADDR = (unsigned char)0x01;
		*USB_BASE_POWER_ADDR = (unsigned char)0x02;
		*USB_INIT_4A_ADDR = (unsigned char)0x20;
		
		*USB_INIT_4B_ADDR = (unsigned char)0x00;
		val = *USB_INIT_3A_ADDR;
		if (val & 0x08)
			*USB_INIT_4B_ADDR = (unsigned char)0x20;
		*USB_BASE_POWER_ADDR = (unsigned char)0x00;
		
		//Wait for 100ms
		timerValue = FiftyMsecTick;
		while (FiftyMsecTick < timerValue+2);
		
		val = *USB_INIT_3A_ADDR;
		if (val & 0x08)
			*USB_BASE_POWER_ADDR = (unsigned char)0x44;
		*USB_BASE_POWER_ADDR = (unsigned char)0xC4;
		
		//Wait on 4C
		i = 0;
		do
		{
			val = *USB_INIT_4C_ADDR;
			
			if (++i > 0xFFFFFF)
				break;
		}while (val != 0x12 && val != 0x52);
		
		if (i < 0xFFFFFF)
		{
			val = *USB_INIT_4D_ADDR; //freak if & 0x20 is true
			val = *USB_INIT_3A_ADDR;
			if (val & 0x08)
			{
				//Do it one way...

				//Wait for 100ms
				timerValue = FiftyMsecTick;
				while (FiftyMsecTick < timerValue+2);
				
				val = *USB_INIT_4D_ADDR ;//freak if 0x40 is true
				
				*USB_INIT_3A_ADDR &= (unsigned char)~0x02;
				*USB_INIT_39_ADDR |= (unsigned char)0x02;
				*USB_INIT_4C_ADDR = (unsigned char)0x08;
				*USB_MODE_ADDR = (unsigned char)0x01;

				//Wait on 4D
				i = 0;
				do
				{
					val = *USB_INIT_4D_ADDR;
					
					if (++i > 0xFFFFFF)
						break;
				}while (!(val & 0x40));
				
				if (i < 0xFFFFFF)
					*USB_INIT_39_ADDR &= (unsigned char)~0x02;
			}
			else
			{
				//Or do it the other way
				*USB_INIT_4C_ADDR = (unsigned char)0x08;
				
				//Wait on 81
				i = 0;
				do
				{
					val = *USB_UNKNOWN_81_ADDR;
					
					if (++i > 0xFFFFFF)
						break;
				}while (val & 0x20);
				
				if (i < 0xFFFFFF)
				{
					*USB_MODE_ADDR = (unsigned char)0x01;

					//Wait for 200ms
					timerValue = FiftyMsecTick;
					while (FiftyMsecTick < timerValue+4);
				}
			}
			
			if (i < 0xFFFFFF)
			{
				//Wrap this up
				*USB_DATA_OUT_EN_ADDR1 = (unsigned char)0xFF;
				*USB_UNKNOWN_92_ADDR = (unsigned char)0x00;
				*USB_DATA_IN_EN_ADDR1 = (unsigned char)0x0E;
				*USB_INIT_RELATED1_ADDR = (unsigned char)0xA1;
				
				//Wait on 81
				i = 0;
				do
				{
					val = *USB_UNKNOWN_81_ADDR;
					
					if (++i > 0xFFFFFF)
						break;
				}while (!(val & 0x40));

				if (i < 0xFFFFFF)
				{
					//"Host bit" should be set now
					val = *USB_MODE_ADDR; //freak if & 0x04 is not true

					//Now wait on the frame counter to start
					for( i = 0; i < 0x5FFFF; i++)
					{
						if ((unsigned char)*USB_FRAME_COUNTER_LOW_ADDR > 0)
							break;
					}

					//Stuff...
					*USB_INT_ENABLE_ADDR = (unsigned char)0x00;
					*USB_UNKNOWN_81_ADDR = (unsigned char)0x08;

					//Wait for 200ms
					timerValue = FiftyMsecTick;
					while (FiftyMsecTick < timerValue+4);
					
					*USB_UNKNOWN_81_ADDR = (unsigned char)0x00;
					
					//Wait for 200ms
					timerValue = FiftyMsecTick;
					while (FiftyMsecTick < timerValue+4);
					
					//Set the address to zero
					*USB_FUNCTION_ADDRESS_ADDR = (unsigned char)0x00;
					
					//Wait for 100ms
					timerValue = FiftyMsecTick;
					while (FiftyMsecTick < timerValue+2);

					//TODO: We should go ahead and get the max packet size and set the address here
					USB_GetMaxPacketSize0();
					
					//Wait for 100ms
					timerValue = FiftyMsecTick;
					while (FiftyMsecTick < timerValue+2);

					USB_SetHostAddress(0x02);
					
					//Make sure we catch the A cable unplugged event later on
					*USB_INT_ENABLE_ADDR = (unsigned char)0x01;
					*USB_INT_MASK_ADDR = (unsigned char)0x20;
				}
			}
		}
	}
	
	if (i >= 0xFFFFFF)
		ret = -1;
	
	SetIntVec(AUTO_INT_5, AutoInt5Backup);
	
	return ret;
}

void USB_HostKill(void)
{
	USB_PeripheralKill(); //maybe?! I think it works for both...
}

void USB_PeripheralInitialize(void)
{
	INT_HANDLER AutoInt5Backup = GetIntVec(AUTO_INT_5);
	SetIntVec(AUTO_INT_5, MyTimerHandler);

	volatile unsigned char val;
	unsigned long i;
	volatile unsigned long timerValue;

	*USB_INT_MASK_ADDR = (unsigned char)0x80;
	
	*USB_INIT_4C_ADDR = (unsigned char)0x00;
	*USB_INT_ENABLE_ADDR = (unsigned char)0x01;
	val = *USB_INIT_4C_ADDR; //dummy read
	*USB_BASE_POWER_ADDR = (unsigned char)0x02;
	*USB_INIT_4A_ADDR = (unsigned char)0x20;

	//Power-related stuff, I believe
	*USB_INIT_4B_ADDR = (unsigned char)0x00;
	if ((unsigned char)*USB_INIT_3A_ADDR & 0x08)
		*USB_INIT_4B_ADDR = (unsigned char)0x20;

	*USB_BASE_POWER_ADDR = (unsigned char)0x00;
	
	//Wait for 100ms
	printf("Waiting for 100ms\n");
	timerValue = FiftyMsecTick;
	while (FiftyMsecTick < timerValue+2);

	if ((unsigned char)*USB_INIT_3A_ADDR & 0x08)
		*USB_BASE_POWER_ADDR = (unsigned char)0x44;
	*USB_BASE_POWER_ADDR = (unsigned char)0xC4;

	*USB_INIT_4C_ADDR = (unsigned char)0x08;
	
	//Wait on 4C
	printf("Waiting on 4C\n");
	i = 0;
	do
	{
		val = *USB_INIT_4C_ADDR;
		
		if (++i > 0xFFFFFF)
			break;
	}while (val != 0x1A && val != 0x5A);
	printf("Done waiting on 4C\n");

	if (i < 0xFFFFFF)
	{
		*USB_DATA_OUT_EN_ADDR1 = (unsigned char)0xFF;
		*USB_UNKNOWN_92_ADDR = (unsigned char)0x00;
		val = *USB_DATA_OUT_EN_ADDR1; //dummy read
		*USB_DATA_IN_EN_ADDR1 = (unsigned char)0x0E;
		*USB_INIT_RELATED1_ADDR = (unsigned char)0x05; //this might determine host vs. peripheral init
	
		if ((unsigned char)*USB_MODE_ADDR & 0x04) //this checks the host bit to see if it's set
		{
			*USB_UNKNOWN_81_ADDR |= (unsigned char)0x02;

			//Wait for 200ms
			timerValue = FiftyMsecTick;
			while (FiftyMsecTick < timerValue+4);
		}
		else
		{
			*USB_UNKNOWN_81_ADDR |= (unsigned char)0x01;
		}
		*USB_BASE_POWER_ADDR |= (unsigned char)0x01;

		//Wait for the frame counter to start
		for( i = 0; i < 0xFFFFFF; i++)
		{
			if ((unsigned char)*USB_FRAME_COUNTER_LOW_ADDR > 0)
				break;
		}

		if (i >= 0xFFFFFF)
			USB_PeripheralKill();
	}
	else
		USB_PeripheralKill();

	//Wait for 200ms
	timerValue = FiftyMsecTick;
	while (FiftyMsecTick < timerValue+4);

	SetIntVec(AUTO_INT_5, AutoInt5Backup);
}

void USB_PeripheralKill(void)
{
	INT_HANDLER AutoInt5Backup = GetIntVec(AUTO_INT_5);
	SetIntVec(AUTO_INT_5, MyTimerHandler);

	volatile unsigned char val;
	unsigned long i = 0;
	volatile unsigned long timerValue;

	//Disable USB interrupts
	*USB_INT_ENABLE_ADDR = (unsigned char)0x00;

	//Is ID line high?
	if ((unsigned char)*USB_INIT_4D_ADDR & 0x20)
	{
		//ID line is high; no cable is connected, or a mini-B cable is connected
		if ((unsigned char)*USB_INIT_4D_ADDR & 0x40)
			//Vbus line is high
			*USB_INIT_4C_ADDR = (unsigned char)0x08;
		else
			//Vbus line is low
			*USB_INIT_4C_ADDR = (unsigned char)0x00;
	}
	else
	{
		//ID line is low; a mini-A cable is connected?
		*USB_INIT_4C_ADDR = (unsigned char)0x00;
	}

	*USB_BASE_POWER_ADDR = (unsigned char)0x02;
	*USB_INIT_39_ADDR &= (unsigned char)0xF8;
	
	*USB_INIT_4C_ADDR = (unsigned char)0x00;

	//Wait for 100ms
	timerValue = FiftyMsecTick;
	while (FiftyMsecTick < timerValue+2);

	if ((unsigned char)*USB_INIT_4D_ADDR & 0x20)
	{
		if ((unsigned char)*USB_INIT_4D_ADDR & 0x10)
		{
			val = *USB_INT_MASK_ADDR;
			*USB_INT_MASK_ADDR = (unsigned char)0x00;
			*USB_INT_MASK_ADDR = val;
		}
		else
		{
			if ((unsigned char)*USB_INIT_4D_ADDR & 0x40)
			{
				*USB_INT_MASK_ADDR = (unsigned char)0x80;
			}
			else
			{
				*USB_INT_MASK_ADDR = (unsigned char)0x50;
			}
		}
	}
	else
	{
		if ((unsigned char)*USB_INIT_4D_ADDR & 0x20)
		{
			val = *USB_INT_MASK_ADDR;
			*USB_INT_MASK_ADDR = (unsigned char)0x00;
			*USB_INT_MASK_ADDR = val;
		}
		else
		{
			i = 0;
			do
			{
				val = *USB_INIT_4D_ADDR;

				if (++i > 0xFFFFFF)
					break;
			}while ((val & 0x81) != 0x81);
		}

		if (i < 0xFFFFFF)
			*USB_INT_MASK_ADDR = (unsigned char)0x22;
	}

	SetIntVec(AUTO_INT_5, AutoInt5Backup);

	//Re-enable USB interrupts
	*USB_INT_ENABLE_ADDR = (unsigned char)0x01;
	*USB_INT_MASK_ADDR = 0xFF;
}
