#include <tigcclib.h>
#include "api.h"

typedef void (*Handle_ReceivingData)(unsigned int size);

void SerialAdapter_Initialize(Handle_ReceivingData receivingData);
void SerialAdapter_Kill(void);
unsigned int SerialAdapter_ReceiveData(unsigned char* buffer, unsigned int size);
void SerialAdapter_SendData(unsigned char* data, unsigned int size);
