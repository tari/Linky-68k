#include <tigcclib.h>
#include "usb.h"

void USB_KillPower()
{
	//Set bit 1, which cuts power to the USB controller
  *USB_BASE_POWER_ADDR = (unsigned char)2;

	//Enable all USB-related interrupts
  *USB_INT_ENABLE_ADDR = (char)1;
  *USB_INT_MASK_ADDR = (char)0xFF;
}

//HACK: I'm pretty sure this routine is completely useless, but I could be wrong
unsigned char USB_OutgoingCmdDummyRead()
{
	volatile unsigned char ret = *USB_OUTGOING_CMD_ADDR;

	return ret;
}

void USB_FinishControlRequest()
{
	*USB_SELECTED_ENDPOINT_ADDR = (unsigned char)0x00;
	USB_OutgoingCmdDummyRead();
	
	*USB_OUTGOING_CMD_ADDR = (unsigned char)0x48;
	USB_OutgoingCmdDummyRead();
}

void USB_FinishControlOutput()
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

int USB_SendBulkData(unsigned char endpoint, unsigned char* data, unsigned int count)
{
	return 0;
}

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

void USB_PeripheralInitialize()
{
	unsigned char val = 0;
	unsigned long i;

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
	OSFreeTimer(0x09);
	OSRegisterTimer(0x09, 2);
	while (!OSTimerExpired(0x09));

	if ((unsigned char)*USB_INIT_3A_ADDR & 0x08)
		*USB_BASE_POWER_ADDR = (unsigned char)0x44;
	*USB_BASE_POWER_ADDR = (unsigned char)0xC4;
	
	*USB_INIT_4C_ADDR = (unsigned char)0x08;
	
	//Wait on 4C
	i = 0;
	do
	{
		val = *USB_INIT_4C_ADDR;
		
		if (++i > 65536)
			break;
	}while (val != 0x1A && val != 0x5A);
	
	if (i < 65536)
	{
		*USB_DATA_OUT_EN_ADDR1 = (unsigned char)0xFF;
		*USB_UNKNOWN_92_ADDR = (unsigned char)0x00;
		val = *USB_DATA_OUT_EN_ADDR1; //dummy read
		*USB_DATA_IN_EN_ADDR1 = (unsigned char)0x0E;
		*USB_INIT_RELATED1_ADDR = (unsigned char)0x05; //this might determine host vs. peripheral init
	
		if ((unsigned char)*USB_MODE_ADDR & 0x04) //this checks the host bit to see if it's set
		{
			*USB_UNKNOWN_81_ADDR |= 0x02;
			//Wait for 200ms
			OSFreeTimer(0x09);
			OSRegisterTimer(0x09, 4);
			while (!OSTimerExpired(0x09));
		}
		else
		{
			*USB_UNKNOWN_81_ADDR |= 0x01;
		}
		*USB_BASE_POWER_ADDR |= 0x01;
		
		//Wait for the frame counter to start
		for( i = 0; i < 0x50000; i++)
			if ((unsigned char)*USB_FRAME_COUNTER_LOW_ADDR > 0)
				break;
		
		if (i >= 0x50000)
			USB_PeripheralKill();
	}
	else
		USB_PeripheralKill();

	//Wait for 200ms
	OSFreeTimer(0x09);
	OSRegisterTimer(0x09, 4);
	while (!OSTimerExpired(0x09));
}

void USB_PeripheralKill()
{
	*USB_INT_ENABLE_ADDR = (unsigned char)0x00;

	if ((unsigned char)*USB_INIT_4D_ADDR & 0x20)
	{
		if ((unsigned char)*USB_INIT_4D_ADDR & 0x40)
			*USB_INIT_4C_ADDR = (unsigned char)0x08;
		else
			*USB_INIT_4C_ADDR = (unsigned char)0x00;
	}
	else
	{
		*USB_INIT_4C_ADDR = (unsigned char)0x00;
	}
	
	*USB_BASE_POWER_ADDR = (unsigned char)0x02;
	*USB_INIT_39_ADDR &= (unsigned char)0xF8;
	
	if ((unsigned char)*USB_INIT_4D_ADDR & 0x20)
	{
		if ((unsigned char)*USB_INIT_4D_ADDR & 0x40)
		{
			*USB_INT_MASK_ADDR = (unsigned char)0x57;
		}
		else
		{
			*USB_INIT_4C_ADDR = (unsigned char)0x00;
			*USB_INT_MASK_ADDR = (unsigned char)0x50;
		}
	}
	else
	{
		unsigned long i = 0;
		unsigned char val;
		do
		{
			val = *USB_INIT_4D_ADDR;
			
			if (++i < 65536)
				break;
		}while ((val & 0x81) > 0);
		
		*USB_INT_MASK_ADDR = (unsigned char)0x22;
	}

	*USB_INT_ENABLE_ADDR = (unsigned char)0x01;
}
