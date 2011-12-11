#ifndef _USB_H
#define _USB_H

//USB Memory-mapped I/O interface
#define USB_INIT_39_ADDR ((volatile unsigned char*)0x00710039)
#define USB_INIT_3A_ADDR ((volatile unsigned char*)0x0071003A)
#define USB_INIT_4A_ADDR ((volatile unsigned char*)0x0071004A)
#define USB_INIT_4B_ADDR ((volatile unsigned char*)0x0071004B)
#define USB_INIT_4C_ADDR ((volatile unsigned char*)0x0071004C)
#define USB_INIT_4D_ADDR ((volatile unsigned char*)0x0071004D)
#define USB_BASE_POWER_ADDR ((volatile unsigned char*)0x00710054)
#define USB_INT_STATUS1_ADDR ((volatile unsigned char*)0x00710055)
#define USB_INT_STATUS2_ADDR ((volatile unsigned char*)0x00710056)
#define USB_INT_MASK_ADDR ((volatile unsigned char*)0x00710057)
#define USB_INT_ENABLE_ADDR ((volatile unsigned char*)0x0071005B)
#define USB_FUNCTION_ADDRESS_ADDR ((volatile unsigned char*)0x00710080)
#define USB_UNKNOWN_81_ADDR ((volatile unsigned char*)0x00710081)
#define USB_OUTGOING_DATA_SUCCESS_ADDR ((volatile unsigned char*)0x00710082)
#define USB_INCOMING_DATA_READY_ADDR ((volatile unsigned char*)0x00710084)
#define USB_DATA_STATUS_ADDR ((volatile unsigned char*)0x00710086)
#define USB_DATA_OUT_EN_ADDR1 ((volatile unsigned char*)0x00710087)
#define USB_DATA_OUT_EN_ADDR2 ((volatile unsigned char*)0x00710088)
#define USB_DATA_IN_EN_ADDR1 ((volatile unsigned char*)0x00710089)
#define USB_DATA_IN_EN_ADDR2 ((volatile unsigned char*)0x0071008A)
#define USB_INIT_RELATED1_ADDR ((volatile unsigned char*)0x0071008B)
#define USB_FRAME_COUNTER_LOW_ADDR ((volatile unsigned char*)0x0071008C)
#define USB_FRAME_COUNTER_HIGH_ADDR ((volatile unsigned char*)0x0071008D)
#define USB_SELECTED_ENDPOINT_ADDR ((volatile unsigned char*)0x0071008E)
#define USB_MODE_ADDR ((volatile unsigned char*)0x0071008F)
#define USB_OUTGOING_MAX_PACKET_SIZE_ADDR ((volatile unsigned char*)0x00710090)
#define USB_OUTGOING_CMD_ADDR ((volatile unsigned char*)0x00710091)
#define USB_UNKNOWN_92_ADDR ((volatile unsigned char*)0x00710092)
#define USB_INCOMING_CMD_ADDR ((volatile unsigned char*)0x00710094)
#define USB_INCOMING_DATA_COUNT_ADDR ((volatile unsigned char*)0x00710096)
#define USB_OUTGOING_PIPE_SETUP_ADDR ((volatile unsigned char*)0x00710098)
#define USB_INCOMING_PIPE_SETUP_ADDR ((volatile unsigned char*)0x0071009A)
#define USB_ENDPOINT0_DATA_ADDR ((volatile unsigned char*)0x007100A0)

//USB peripheral descriptors
/*struct DeviceDescriptor
{
	short bcdUSB;
	unsigned char bDeviceClass;
	unsigned char bDeviceSubClass;
	unsigned char bDeviceProtocol;
	unsigned char bMaxPacketSize0;
	short idVendor;
	short idProduct;
	short bcdDevice;
	unsigned char iManufacturer;
	unsigned char iProduct;
	unsigned char iSerialNumber;
	unsigned char bNumConfigurations;
};*/

typedef enum
{
	Type_Isochronous = 0x10,
	Type_Bulk = 0x20,
	Type_Interrupt = 0x30
} USB_EndpointType;

//USB peripheral callbacks
typedef void (*Handle_SetConfiguration)(void);
typedef void (*Handle_Connected)(void);
typedef void (*Handle_HubSetFeature)(int feature, int index);
typedef void (*Handle_HubClearFeature)(int feature, int index);
typedef void (*Handle_HubGetStatus)(unsigned char* data, int index);
typedef void (*Handle_SetAddress)(int address);
typedef void (*Handle_ControlOutputDone)(void);
typedef int (*Handle_UnknownControlRequest)(unsigned char bmRequestType, unsigned char bRequest, unsigned int wValue, unsigned int wIndex, unsigned int wLength);
typedef void (*Handle_IncomingData)(unsigned char readyMap);

//USB peripheral mode interface
typedef struct
{
	const unsigned char*         deviceDescriptor;
	const unsigned char*         configDescriptor;
	Handle_SetConfiguration      h_setConfig;
	Handle_Connected             h_connected;
	Handle_HubSetFeature         h_hubSetFeature;
	Handle_HubClearFeature       h_hubClearFeature;
	Handle_HubGetStatus          h_hubGetStatus;
	Handle_SetAddress            h_setAddress;
	Handle_ControlOutputDone     h_controlOutputDone;
	Handle_UnknownControlRequest h_unknownControlRequest;
	Handle_IncomingData          h_incomingData;
} USBPeripheral;

const USBPeripheral DEFAULT_USB_PERIPHERAL = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

typedef struct
{
	unsigned int bFunctionAddress;
	unsigned int bParentAddress;
	unsigned int bHubPortNumber;
} USBDevice;

//For internal driver use (should these go somewhere else?)
extern USBPeripheral* peripheralInterface;         //Pointer to USB peripheral mode setup/interface information
extern INT_HANDLER OldInt3;                        //Backup of old AUTO_INT_3 interrupt vector
extern unsigned char bMaxPacketSize0;				      //used in control pipe communication
extern const unsigned char* controlDataAddress;		//used in sending back control request responses
extern unsigned int responseBytesRemaining;	      //used in sending back control request responses
extern int USBState;													      //used in sending back control request responses
extern int newAddressReceived;								      //used in setting the address from the interrupt
extern int wAddress;													      //used in setting the address from the interrupt
extern int bytesBuffered[0x0F];							      //keeps track of buffered incoming data per pipe
extern unsigned char incomingDataReadyMap;					//keeps track of incoming data per pipe

//USB functions
void USB_HandleInterrupt(void);
int USB_HostInitialize(void);
void USB_HostKill(void);
int USB_IsDataReady(unsigned char endpoint);
void USB_KillPower(void);
void USB_PeripheralInitialize(void);
void USB_PeripheralKill(void);
void USB_SetFunctionAddress(int address);
void USB_FinishControlRequest(void);
void USB_StartControlOutput(const unsigned char* address, int bytesRemaining);
void USB_SendControlData(unsigned char* data, unsigned int length);
void USB_ReceiveControlData(unsigned char* data, unsigned int length);
void USB_FinishControlOutput(void);
int USB_SendInterruptData(unsigned char endpoint, unsigned char* data, unsigned int count);
int USB_ReceiveInterruptData(unsigned char endpoint, unsigned char* data, unsigned int count);
void USB_SendBulkData(unsigned char endpoint, unsigned char* data, unsigned int count);
int USB_ReceiveBulkData(unsigned char endpoint, unsigned char* data, unsigned int count);
void USB_SetupOutgoingPipe(unsigned char endpoint, USB_EndpointType type, unsigned char maxPacketSize);
unsigned char USB_GetDescriptor(unsigned char type, unsigned char* responseBuffer, unsigned int bytesExpectedToReceive);
unsigned char USB_GetDeviceDescriptor(unsigned char* responseBuffer, unsigned int bufferLength);
unsigned char USB_GetConfigurationDescriptor(unsigned char* responseBuffer, unsigned int bufferLength);

#endif
