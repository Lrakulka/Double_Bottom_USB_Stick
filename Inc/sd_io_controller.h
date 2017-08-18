/* This file overrides and extends default middleware logic of the file sd_diskio.h */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SD_IO_CONT_H
#define __SD_IO_CONT_H

#include "ff_gen_drv.h"
#include "fatfs.h"

#define CONF_KEY_LENGHT          20
#define PART_KEY_LENGHT          20
#define ROOT_KEY_LENGHT          40
#define PART_NAME_LENGHT         20

#define MAX_PART_NUMBER          10

#define STORAGE_SECTOR_NUMBER    2

#define PUBLIC_PARTITION_KEY    "public"

#define DEVICE_UNIQUE_ID				"deviceUniqueID"		// Should be unique for each device and fit in ROOT_KEY_LENGHT
// Partition encryption
typedef enum {
  NOT_INITIALIZED = 0,  
  INITIALIZED
} InitStatus;

typedef enum {
  ENCRYPTED = 0,  
  NOT_ENCRYPTED,
  DECRYPTED
} EncryptionType;

typedef struct {
   DWORD startSector;
   DWORD lastSector;
   UINT sectorNumber;
   char name[PART_NAME_LENGHT];                   	// Partition name must be less than 21 symbols
   char key[PART_KEY_LENGHT];                   		// Partition key must be less than 21 symbols
   EncryptionType encryptionType;
} Partition;

typedef struct {
   Partition partitions[MAX_PART_NUMBER];
   uint8_t partitionsNumber;
   uint8_t currPartitionNumber;   
   InitStatus initializeStatus;    
   char confKey[CONF_KEY_LENGHT];                   // Key for revealing the current configurations of the device
   char rootKey[ROOT_KEY_LENGHT];                   // General password to enter to hidden partitions
} PartitionsStructure;

extern Diskio_drvTypeDef  SD_Driver;

DSTATUS initSDCard(void);
uint8_t changePartition(const char*, const char*);
// Operations with current visible partition
int8_t currentPartitionCapacity(uint32_t*, uint16_t*);
int8_t currentPartitionInit(void);
int8_t currentPartitionisReady(void);
int8_t currentPartitionIsWriteProtected(void);
int8_t currentPartitionMaxLun(void);
int8_t currentPartitionRead(BYTE*, DWORD, UINT);
int8_t currentPartitionWrite(BYTE*, DWORD, UINT);

uint8_t setConf(PartitionsStructure*, const PartitionsStructure*);
uint8_t loadConf(PartitionsStructure*, const char*);

uint8_t initStartConf();
#endif
