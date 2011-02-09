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
		} while ((val & (endpoint << 1)) == 0);
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
		unsigned char bytesInQueue = bytesBuffered[endpoint & 0x0F];
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

		bytesInQueue -= ret;
		bytesBuffered[endpoint & 0x0F] = bytesInQueue;

		if (bytesInQueue <= 0x00)
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

void USB_SendControlData(unsigned char* data, unsigned int length)
{
	//Do nothing
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

unsigned char USB_SendControlRequest(unsigned char* request, unsigned char* responseBuffer, unsigned char bytesExpectedToReceive)
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
			count = bytesExpectedToReceive > 0x40 ? 0x40 : bytesExpectedToReceive; //HACK: assuming bMaxPacketSize0 is 0x40
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

unsigned char USB_GetDescriptor(unsigned char type, unsigned char* responseBuffer, unsigned int bytesExpectedToReceive)
{
	unsigned char request[] = {0x80, 0x06, 0x00, type, 0x00, 0x00, (bytesExpectedToReceive & 0xFF), (bytesExpectedToReceive >> 8)};
	
	USB_SendControlRequest(request, responseBuffer, bytesExpectedToReceive);
	
	return 0;
}

unsigned char USB_SetHostAddress(unsigned char address)
{
	//Buffer the request data
	unsigned char request[] = {0x00, 0x05, address, 0x00, 0x00, 0x00, 0x00, 0x00};
	
	USB_SendControlRequest(request, NULL, 0);
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
					val = *USB_INIT_4C_ADDR;
					
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
	timerValue = FiftyMsecTick;
	while (FiftyMsecTick < timerValue+2);

	if ((unsigned char)*USB_INIT_3A_ADDR & 0x08)
		*USB_BASE_POWER_ADDR = (unsigned char)0x44;
	*USB_BASE_POWER_ADDR = (unsigned char)0xC4;

	*USB_INIT_4C_ADDR = (unsigned char)0x08;
	
	//Wait on 4C
	i = 0;
	do
	{
		val = *USB_INIT_4C_ADDR;
		
		if (++i > 0xFFFFFF)
			break;
	}while (val != 0x1A && val != 0x5A);
	
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
}
