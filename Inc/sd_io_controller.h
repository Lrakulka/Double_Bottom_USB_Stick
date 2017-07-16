/* This file overrides default middleware loggic in the file sd_diskio.h */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SD_IO_H
#define __SD_IO_H

#include "ff_gen_drv.h"

extern Diskio_drvTypeDef  SD_Driver;

DSTATUS initControllerMemory(void);
// Operations with current visible partition
int8_t currentPartitionCapacity(uint32_t*, uint16_t*);
int8_t currentPartitionInit(void);
int8_t currentPartitionisReady(void);
int8_t currentPartitionIsWriteProtected(void);
int8_t currentPartitionMaxLun(void);
int8_t currentPartitionRead(BYTE*, DWORD, UINT);
int8_t currentPartitionWrite(BYTE*, DWORD, UINT);
	
#endif
