/* This file overrides default middleware loggic in the file sd_diskio.h */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SD_IO_CONT_H
#define __SD_IO_CONT_H

#include "ff_gen_drv.h"

/* This sequence uses for correctness check after initialization of main partition structure*/
#define CHECK_SEQUENCE                      "12d@fh%fbdfb6B37b=-!"
/* This define should be unique for earch device. 
 * UNIQUE_DEVICE_INDICATOR uses for encryption of default partition config.
*/
#define UNIQUE_DEVICE_INDICATOR             "we23dcf!segf&dg?d3rf"
/* This string uses for checking correctness of loading default partition configs*/
#define DEVICE_NAME                         "TestProrotype"

#define CONF_KEY_LENGHT                     20
#define PART_KEY_LENGHT                     20
#define PART_NAME_LENGHT                    20
#define ROOT_KEY_LENGHT                     40
#define SHOW_CONF_KEY_LENGHT                20

#define MAX_PART_NUMBER                     10

// Storage number of sectors for default zero partition
#define STORAGE_DEFAULT_PART_SECTOR_NUMBER  1
// Storage number of sectors for configurations
#define STORAGE_SECTOR_NUMBER               2 + STORAGE_DEFAULT_PART_SECTOR_NUMBER  

#define PUBLIC_PARTITION_KEY    "public"
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
   char name[PART_NAME_LENGHT];                  // Partition name must be less than 21 symbols
   char key[PART_KEY_LENGHT];                    // Partition key must be less than 21 symbols
   EncryptionType encryptionType;
} Partition;

typedef struct {
   Partition partitions[MAX_PART_NUMBER];
   uint8_t partitionsNumber;
   uint8_t currPartitionNumber;   
   InitStatus isInitilized;    
   char confKey[CONF_KEY_LENGHT];                   // Key for revealing the current configurations of the device
   char rootKey[ROOT_KEY_LENGHT];                   // General password to enter to hidden partitions
   char checkSequence[sizeof(CHECK_SEQUENCE)];      // Uses for correctness check of decrypting
} PartitionsStructure;

extern Diskio_drvTypeDef  SD_Driver;

DSTATUS initControllerMemory(void);
uint8_t changePartition(const char*, const char*);
// Operations with current visible partition
int8_t currentPartitionCapacity(uint32_t*, uint16_t*);
int8_t currentPartitionInit(void);
int8_t currentPartitionisReady(void);
int8_t currentPartitionIsWriteProtected(void);
int8_t currentPartitionMaxLun(void);
int8_t currentPartitionRead(BYTE*, DWORD, UINT);
int8_t currentPartitionWrite(BYTE*, DWORD, UINT);

uint8_t setConf(const PartitionsStructure*);
#endif
