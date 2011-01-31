#include <tigcclib.h>
#include "usb.h"

typedef unsigned char* (*Handle_ReadSectorRequest)(unsigned long long int LBA);
typedef void (*Handle_WriteSectorRequest)(unsigned long long int LBA, const unsigned char* sectorData);

void MassStorage_Initialize(Handle_ReadSectorRequest readSector, Handle_WriteSectorRequest writeSector);
void MassStorage_Kill(void);
void MassStorage_Do(void);
