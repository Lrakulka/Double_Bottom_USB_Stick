/**
  ******************************************************************************
  * @file           : SD_IO_CONTROLLER
  * @version        : v1.0
  * @brief          : Header for sd_io_controller file
  ******************************************************************************
  * MIT License
  *
  * Copyright (c) 2017 Oleksandr Borysov
  *
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to deal
  * in the Software without restriction, including without limitation the rights
  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  * copies of the Software, and to permit persons to whom the Software is
  * furnished to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all
  * copies or substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  * SOFTWARE.
 ******************************************************************************
*/

/* This file overrides and extends default middleware logic of the file sd_diskio.h */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SD_IO_CONT_H
#define __SD_IO_CONT_H

#include "ff_gen_drv.h"
#include "fatfs.h"
// For debugging
#define DEBUG_MOD									1									// if DEBUG_MOD != 0 the command file will not deleted
#define CIPHER_MOD								1									// if CIPHER_MOD != 0 the cipher logic not active

#define CONF_KEY_LENGHT          20
#define PART_KEY_LENGHT          20
#define ROOT_KEY_LENGHT          40

#define PART_NAME_LENGHT         20

#define MAX_PART_NUMBER          10

#define STORAGE_SECTOR_NUMBER    2

#define PUBLIC_PARTITION_KEY    "public"

#define DEVICE_UNIQUE_ID				"deviceUniqueID"		// Should be unique for each device and fit in ROOT_KEY_LENGHT
// Device configuration status
typedef enum {
  NOT_INITIALIZED = 0,  
  INITIALIZED
} InitStatus;
// Partition encryption status
typedef enum {
  PUBLIC = 0,																				// Uses for public partitions
  PRIVATE,																					// Uses for private partitions
} PartitionType;
// Partition configurations
typedef struct {
   DWORD startSector;
   DWORD lastSector;
   UINT sectorNumber;
   char name[PART_NAME_LENGHT];                   	// Partition name must be less than 21 symbols
   char key[PART_KEY_LENGHT];                   		// Partition key must be less than 21 symbols
   PartitionType partitionType;
} Partition;
// Device configurations
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
// Operations with currently visible partition
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
