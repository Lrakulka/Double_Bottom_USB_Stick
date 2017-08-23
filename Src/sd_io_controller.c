/**
  ******************************************************************************
  * @file           : SD_IO_CONTROLLER
  * @version        : v1.0
  * @brief          : This file implements the sd_io_controller
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

/* This file overrides default middleware loggic in the file sd_diskio.c */

/* Includes ------------------------------------------------------------------*/
#include "sd_io_controller.h"
#include <string.h>
#include "ff_gen_drv.h"
#include "usbd_storage_if.h"
#include "aes.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
/* Block Size in Bytes */
#define STORAGE_BLOCK_SIZE               512
#define STORAGE_LUN_NBR                  0  

#define ECB                              1              // Enable both ECB
                                                        // Define length of AES encryption key
#if defined AES128
  #define AES_KEY_SIZE                   16
#elif defined AES192
  #define AES_KEY_SIZE                   24
#elif defined AES256
  #define AES_KEY_SIZE                   32
#else
  #define AES_KEY_SIZE                   -1
#endif

/* Disk Status Bits (DSTATUS) */
#define STA_INIT      0x00  /* Drive initialized */
#define STA_NOINIT    0x01  /* Drive not initialized */
#define STA_NODISK    0x02  /* No medium in the drive */
#define STA_PROTECT   0x04  /* Write protected */

/* Private variables ---------------------------------------------------------*/
PartitionsStructure partitionsStructure;                  // Contains current device configurations
char longPartXORkey[STORAGE_BLOCK_SIZE];                  // The password key for XOR cipher
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

extern HAL_SD_CardInfoTypedef SDCardInfo;
// Timer interrupt for the command file scan
extern TIM_HandleTypeDef htim14;

/* Private controller function prototypes -----------------------------------------------*/
// Operations with current partition
DWORD getPartitionSector(DWORD);
uint8_t isPartitionContainsMemorySectors(DWORD, UINT);
Partition* getPartition(void);
void decryptMemoryAES(BYTE*, const char*, const uint32_t);
void encryptMemoryAES(BYTE*, const char*, const uint32_t);
void decryptMemory(BYTE*, const char*, const uint32_t);
void encryptMemory(BYTE*, const char*, const uint32_t);
void cipherXOR(BYTE*, const char*, const uint32_t);
void createKeyWithSpecLength(const char*, char*, const uint16_t);
uint8_t checkNewPartitionsStructure(const PartitionsStructure*);
uint8_t saveConf(const PartitionsStructure*);
void resetTimerInerrupt(void);

/* Private SD Card function prototypes -----------------------------------------------*/
DSTATUS SD_initialize (BYTE);
DSTATUS SD_status (BYTE);
DRESULT SD_read (BYTE, BYTE*, DWORD, UINT);
DRESULT SD_write (BYTE, const BYTE*, DWORD, UINT);
DRESULT SD_ioctl (BYTE, BYTE, void*);
  
Diskio_drvTypeDef  SD_Driver =
{
  SD_initialize,
  SD_status,
  SD_read, 
  SD_write,
  SD_ioctl,
};

/* SD card controller logic ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  lun : not used 
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_initialize(BYTE lun) {
  if ((Stat == STA_INIT) || (BSP_SD_Init() == MSD_OK)) {
    Stat = STA_INIT;
  }
  
  return Stat;
}

/**
  * @brief  Gets Disk Status
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_status(BYTE lun) {
  Stat = STA_INIT;

  if(BSP_SD_GetStatus() != MSD_OK) {
    Stat = STA_NOINIT;
  }
  
  return Stat;
}

/**
  * @brief  Reads Sector(s)
  * @param  lun : not used
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count) {
  DRESULT res = RES_ERROR;
  
  DWORD shiftedSector = getPartitionSector(sector);
  if (isPartitionContainsMemorySectors(shiftedSector, count)
    && (BSP_SD_ReadBlocks_DMA((uint32_t*) buff, 
          (uint64_t) (shiftedSector * STORAGE_BLOCK_SIZE), 
          STORAGE_BLOCK_SIZE, count * SDCardInfo.CardBlockSize / STORAGE_BLOCK_SIZE) == MSD_OK)) {
#if  CIPHER_MOD == 0
    if (getPartition()->partitionType == PRIVATE) {
      decryptMemory(buff, longPartXORkey, count * SDCardInfo.CardBlockSize);
    }
#endif
    res = RES_OK;
  }
  
  return res;
}

/**
  * @brief  Writes Sector(s)
  * @param  lun : not used
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count) {
  DRESULT res = RES_ERROR;
  
  DWORD shiftedSector = getPartitionSector(sector);
#if  CIPHER_MOD == 0
  if (getPartition()->partitionType == PRIVATE) {
    encryptMemory((BYTE*) buff, longPartXORkey, count * SDCardInfo.CardBlockSize);
  }
#endif
  if (isPartitionContainsMemorySectors(shiftedSector, count)
    && (BSP_SD_WriteBlocks_DMA((uint32_t*) buff,
          (uint64_t) (shiftedSector * STORAGE_BLOCK_SIZE), 
          STORAGE_BLOCK_SIZE, count * SDCardInfo.CardBlockSize / STORAGE_BLOCK_SIZE) == MSD_OK)) {
            
    res = RES_OK;
  }
  
  return res;
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  lun : not used
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff) {
  DRESULT res = RES_ERROR;
  //SD_status(0);
  if (Stat & STA_NOINIT) {
    return RES_NOTRDY;
  }
  
  switch (cmd) {
  /* Make sure that no pending write process */
  case CTRL_SYNC :
    res = RES_OK;
    break;
  
  /* Get number of sectors on the disk (DWORD) */
  case GET_SECTOR_COUNT :
    //BSP_SD_GetCardInfo(&CardInfo);
    *(DWORD*)buff = getPartition()->sectorNumber;
    res = RES_OK;
    break;
  
  /* Get R/W sector size (WORD) */
  case GET_SECTOR_SIZE :
    *(WORD*)buff = STORAGE_BLOCK_SIZE;
    res = RES_OK;
    break;
  
  /* Get erase block size in unit of sector (DWORD) */
  case GET_BLOCK_SIZE :
    *(DWORD*)buff = STORAGE_BLOCK_SIZE;
    break;
  
  default:
    res = RES_PARERR;
  }
  
  return res;
}

/* USB interface logic ---------------------------------------------------------*/

/*******************************************************************************
* Description    : Gets capacity of currently visible partition.
* Input          : None.
* Output         : block_num - number of the memory blocks
*                  block_size - size of the memory block.
* Return         : 0 if success or error code.
*******************************************************************************/
int8_t currentPartitionCapacity(uint32_t *block_num, uint16_t *block_size) {
  resetTimerInerrupt();
  *block_size = STORAGE_BLOCK_SIZE;
  if (partitionsStructure.initializeStatus == INITIALIZED) {
    *block_num = getPartition()->sectorNumber;
  } else {                                                           // If the configurations not initialized
     FATFS *fs;
     DWORD fre_clust;
     if (f_getfree((TCHAR const*)SD_Path, &fre_clust, &fs) == FR_OK) {
       *block_num = (fs->n_fatent - 2) * fs->csize;                  // Trying to get capacity by FatFs
       getPartition()->sectorNumber = *block_num;
       getPartition()->lastSector = *block_num - 1;
     } else {
       *block_num = SDCardInfo.CardCapacity / STORAGE_BLOCK_SIZE - 1;// Get capacity of SD card
     }
  }
  return USBD_OK;
}

/*******************************************************************************
* Description    : Reads data from the storage.
* Input          : sector - start address of the memory
*                  count - number of the memory blocks to read.
* Output         : buff - a part of the memory.
* Return         : 0 if success or error code.
*******************************************************************************/
int8_t currentPartitionRead(BYTE *buff, DWORD sector, UINT count) {
  resetTimerInerrupt();                                              // Reset Timer for the command file scan
  return SD_read(STORAGE_LUN_NBR, buff, sector, count);
}

/*******************************************************************************
* Description    : Writes data from the storage.
* Input          : sector - start address of the memory
*                  count - number of the memory blocks to write
*                  buff - memory to write.
* Output         : None.
* Return         : 0 if success or error code.
*******************************************************************************/
int8_t currentPartitionWrite(BYTE *buff, DWORD sector, UINT count) {
  resetTimerInerrupt();                                            // Reset Timer for the command file scan
  return SD_write(STORAGE_LUN_NBR, buff, sector, count);
}

/*******************************************************************************
* Description    : Initializes current partition.
* Input          : None.
* Output         : None.
* Return         : 0.
*******************************************************************************/
int8_t currentPartitionInit(void) {
  resetTimerInerrupt();                                           // Reset Timer for the command file scan
  return USBD_OK;
}

/*******************************************************************************
* Description    : Returns the partition status.
* Input          : None.
* Output         : None.
* Return         : 0.
*******************************************************************************/
int8_t currentPartitionisReady(void) {
  resetTimerInerrupt();                                          // Reset Timer for the command file scan
  return USBD_OK;
}

/*******************************************************************************
* Description    : Returns the partition protection status.
* Input          : None.
* Output         : None.
* Return         : 0.
*******************************************************************************/
int8_t currentPartitionIsWriteProtected(void) {
  resetTimerInerrupt();                                         // Reset Timer for the command file scan
  return USBD_OK;
}
  
/*******************************************************************************
* Description    : Returns the number of LUN.
* Input          : None.
* Output         : None.
* Return         : 0.
*******************************************************************************/
int8_t currentPartitionMaxLun(void) {
  resetTimerInerrupt();                                        // Reset Timer for the command file scan
  return STORAGE_LUN_NBR;
}
//-------------------------------------------------------------------------------------------------
// Controller partition logic

/*******************************************************************************
* Description    : Init SD Card and initialized the device with default configuration.
* Input          : None.
* Output         : None.
* Return         : 0 id success or 1 if not.
*******************************************************************************/
DSTATUS initSDCard(void) {
  DSTATUS res = RES_ERROR;
  if (SD_initialize(STORAGE_LUN_NBR) == RES_OK) {
                                                              // Default configuration
    partitionsStructure.partitionsNumber = 1;
    partitionsStructure.currPartitionNumber = 0;
    strcpy(getPartition()->name, "partDefault");
    getPartition()->startSector = 0x0;
    getPartition()->lastSector = SDCardInfo.CardCapacity / STORAGE_BLOCK_SIZE - STORAGE_SECTOR_NUMBER - 2;
    getPartition()->sectorNumber = getPartition()->lastSector + 1;
    res = RES_OK;
  }
  return res;
}

/*******************************************************************************
* Description    : Switch current visible partition to another.
* Input          : partName - name of the partition that will set to be visible
*                  partKey - the partition key.
* Output         : None.
* Return         : 0 id success or 1 if not.
*******************************************************************************/
uint8_t changePartition(const char *partName, const char *partKey) {
  uint8_t res = 1;
  for (uint8_t partNmb = 0; partNmb < partitionsStructure.partitionsNumber; ++partNmb) {
    if ((strncmp(partName, partitionsStructure.partitions[partNmb].name, PART_NAME_LENGHT) == 0)
        && ((partitionsStructure.partitions[partNmb].partitionType == PUBLIC)
            || (strncmp(partKey, partitionsStructure.partitions[partNmb].key, PART_KEY_LENGHT) == 0))) {
      partitionsStructure.currPartitionNumber = partNmb;
#if  CIPHER_MOD == 0
      createKeyWithSpecLength(getPartition()->key, longPartXORkey, STORAGE_BLOCK_SIZE);
#endif
      res = 0;
      break;
    }
  }
  return res;
}

/*******************************************************************************
* Description    : Sets new configurations to the device configuration.
* Input          : oldConf - name of the device configuration structure
*                  newConf - name of the new configuration for the device.
* Output         : None.
* Return         : 0 id success or 1 if not.
*******************************************************************************/
uint8_t setConf(PartitionsStructure *oldConf, const PartitionsStructure *newConf) {
  uint8_t res = checkNewPartitionsStructure(newConf);
  if (res == 0) {
    *oldConf = *newConf;
    oldConf->initializeStatus = INITIALIZED;
    res = saveConf(oldConf);
  }
  return res;
}

/* Private controller functions ---------------------------------------------------------*/

/*******************************************************************************
* Description    : Saves configuration to the storage.
* Input          : partitionsStructure - the device configuration to save on storage.
* Output         : None.
* Return         : 0 id success or 1 if not.
*******************************************************************************/
uint8_t saveConf(const PartitionsStructure *partitionsStructure) {
  uint8_t res = 1;
  uint32_t memorySize = STORAGE_BLOCK_SIZE * STORAGE_SECTOR_NUMBER;
  uint64_t storageAddr = SDCardInfo.CardCapacity - STORAGE_SECTOR_NUMBER * STORAGE_BLOCK_SIZE;
  BYTE alignMemory[memorySize];
  memcpy(alignMemory, partitionsStructure, sizeof(*partitionsStructure));
#if  CIPHER_MOD == 0
  encryptMemoryAES(alignMemory, partitionsStructure->rootKey, memorySize);
#endif
  
  if (BSP_SD_WriteBlocks_DMA((uint32_t*) alignMemory,
      storageAddr, STORAGE_BLOCK_SIZE, 
      STORAGE_SECTOR_NUMBER * SDCardInfo.CardBlockSize / STORAGE_BLOCK_SIZE) == MSD_OK) {
    res = 0;
  }
  return res;
}

/*******************************************************************************
* Description    : Loads the device configuration from the storage.
* Input          : rootKey - root key to encrypt the device configuration.
* Output         : partitionsStructure - the device configuration.
* Return         : 0 id success or 1 if not.
*******************************************************************************/
uint8_t loadConf(PartitionsStructure *partitionsStructure, const char *rootKey) {
  PartitionsStructure newConfStructure;
  uint8_t res = 1;
  uint32_t memorySize = STORAGE_BLOCK_SIZE * STORAGE_SECTOR_NUMBER;
  uint64_t storageAddr = SDCardInfo.CardCapacity - STORAGE_SECTOR_NUMBER * STORAGE_BLOCK_SIZE;
  BYTE alignMemory[memorySize];
  
  if (BSP_SD_ReadBlocks_DMA((uint32_t*) alignMemory, storageAddr, 
      STORAGE_BLOCK_SIZE, STORAGE_SECTOR_NUMBER * SDCardInfo.CardBlockSize / STORAGE_BLOCK_SIZE) == MSD_OK) {
#if  CIPHER_MOD == 0
    decryptMemoryAES(alignMemory, rootKey, memorySize);
#endif
    memcpy((void*) &newConfStructure, alignMemory, sizeof(newConfStructure));
    // Check data correctness
    if (strncmp(newConfStructure.rootKey, rootKey, ROOT_KEY_LENGHT) == 0) {
      res = 0;
      *partitionsStructure = newConfStructure;
    }
  }
  return res;
}

/*******************************************************************************
* Description    : Checks the device new configurations for errors.
* Input          : partitionStructure - the device configuration which will be checked for correctness.
* Output         : None.
* Return         : 0 id success or 1 if not.
*******************************************************************************/
uint8_t checkNewPartitionsStructure(const PartitionsStructure *partitionStructure) {
  uint32_t blockUsed = 0;
  if ((partitionStructure->confKey[0] == '\0') 
      || (partitionStructure->rootKey[0] == '\0')) {
    return 1;
  }
  for (uint8_t i = 0; i < partitionStructure->partitionsNumber; ++i) {
    if ((partitionStructure->partitions[i].name[0] == '\0') 
      || (partitionStructure->partitions[i].key[0] == '\0')
      || ((partitionStructure->partitions[i].partitionType == PUBLIC)
          && (strncmp(partitionStructure->partitions[i].key,
              PUBLIC_PARTITION_KEY, PART_KEY_LENGHT) != 0))
      || (partitionStructure->partitions[i].lastSector != partitionStructure->partitions[i].startSector 
          + partitionStructure->partitions[i].sectorNumber - 1)) {
            return 1;
          }
      blockUsed += partitionStructure->partitions[i].sectorNumber;
    }
  if (blockUsed > SDCardInfo.CardCapacity / SDCardInfo.CardBlockSize - STORAGE_SECTOR_NUMBER) {
    return 1;
  }
  return 0;
}

/*******************************************************************************
* Description    : Initializes starting configurations for device .
* Input          : None.
* Output         : None.
* Return         : 0 id success or 1 if not.
*******************************************************************************/
uint8_t initStartConf() {
  partitionsStructure.partitionsNumber = 2;
  partitionsStructure.currPartitionNumber = 0;
  strcpy(partitionsStructure.partitions[0].name, "part0");
  partitionsStructure.partitions[0].startSector = 0x0;
  partitionsStructure.partitions[0].lastSector = (SDCardInfo.CardCapacity / STORAGE_BLOCK_SIZE) / 2;
  partitionsStructure.partitions[0].sectorNumber = partitionsStructure.partitions[0].lastSector + 1;
  partitionsStructure.partitions[0].partitionType = PUBLIC;

  memset(partitionsStructure.partitions[1].name, '\0', sizeof(partitionsStructure.partitions[1].name));
  memset(partitionsStructure.partitions[1].key, '\0', sizeof(partitionsStructure.partitions[1].key));
  strcpy(partitionsStructure.partitions[1].name, "part1");
  strcpy(partitionsStructure.partitions[1].key, "part1Key");
  partitionsStructure.partitions[1].startSector = partitionsStructure.partitions[0].lastSector + 1;
  partitionsStructure.partitions[1].lastSector = SDCardInfo.CardCapacity / STORAGE_BLOCK_SIZE
      - STORAGE_SECTOR_NUMBER - 1;
  partitionsStructure.partitions[1].sectorNumber = partitionsStructure.partitions[1].lastSector
      - partitionsStructure.partitions[1].startSector + 1;
  partitionsStructure.partitions[1].partitionType = PRIVATE;

  strcpy(partitionsStructure.confKey, "confKey");
  strcpy(partitionsStructure.rootKey, "rootKey");

  partitionsStructure.initializeStatus = INITIALIZED;
  return saveConf(&partitionsStructure);
}

/*******************************************************************************
* Description    : Calculates sector address with respect to the currently visible partition.
* Input          : sector - desired sector.
* Output         : None.
* Return         : Sector with respect of current visible partition.
*******************************************************************************/
DWORD getPartitionSector(DWORD sector) {
  return sector + getPartition()->startSector;
}

/*******************************************************************************
* Description    : Checks desired memory to be in the currently visible partition.
* Input          : shiftedSector - start sector of the desired memory
*                  count - number of memory blocks.
* Output         : None.
* Return         : True if requested sector contains in current visible partition.
*******************************************************************************/
uint8_t isPartitionContainsMemorySectors(DWORD shiftedSector, UINT count) {
  shiftedSector += count;
  return (shiftedSector >= getPartition()->startSector) && (shiftedSector <= getPartition()->lastSector) ? 1 : 0;
}

/*******************************************************************************
* Description    : Checks desired memory to be in the currently visible partition.
* Input          : None.
* Output         : None.
* Return         : Current visible partition configurations.
*******************************************************************************/
Partition* getPartition() {
  return &partitionsStructure.partitions[partitionsStructure.currPartitionNumber];
}

/*******************************************************************************
* Description    : Decrypt memory block encrypted by AES cipher.
* Input          : buff - data to decrypt
*                  key - decryption key
*                  size - size of the buffer.
* Output         : buff - decrypted data.
* Return         : None.
*******************************************************************************/
void decryptMemoryAES(BYTE *buff, const char *key, const uint32_t size) {
  char keyP[AES_KEY_SIZE];
  BYTE buf[AES_KEY_SIZE];

  createKeyWithSpecLength(key, keyP, AES_KEY_SIZE);

  for(uint32_t i = 0; i < size / AES_KEY_SIZE; ++i) {
      memset(buf, 0, AES_KEY_SIZE);
      AES_ECB_decrypt(buff + (i * AES_KEY_SIZE), (uint8_t*) keyP, buf, AES_KEY_SIZE);
      memcpy(buff + (i * AES_KEY_SIZE), buf, AES_KEY_SIZE);
  }
}

/*******************************************************************************
* Description    : Encrypt memory block with AES cipher.
* Input          : buff - data to encrypt
*                  key - encryption key
*                  size - size of the buffer.
* Output         : buff - encrypted data.
* Return         : None.
*******************************************************************************/
void encryptMemoryAES(BYTE *buff, const char *key, const uint32_t size) {
  char keyP[AES_KEY_SIZE];
  BYTE buf[AES_KEY_SIZE];

  createKeyWithSpecLength(key, keyP, AES_KEY_SIZE);

  for(uint32_t i = 0; i < size / AES_KEY_SIZE; ++i) {
      memset(buf, 0, AES_KEY_SIZE);
      AES_ECB_encrypt(buff + (i * AES_KEY_SIZE), (uint8_t*) keyP, buf, AES_KEY_SIZE);
      memcpy(buff + (i * AES_KEY_SIZE), buf, AES_KEY_SIZE);
  }
}

/*******************************************************************************
* Description    : Creates key with specific length
*                   The algorithm of forming new key should be more complicated to provide additional protection.
* Input          : keyIn - base key that use to form new key
*                  keyOutLength - desired key length.
* Output         : keyOut - key with specific length.
* Return         : None.
*******************************************************************************/
void createKeyWithSpecLength(const char *keyIn, char *keyOut, const uint16_t keyOutLength) {
  uint8_t keyInLength = strlen(keyIn);
  if (keyInLength == 0) {
    memset(keyOut, '\0', keyOutLength);
    return;
  }
  for (uint16_t i = 0; i < keyOutLength; ++i) {
    keyOut[i] = keyIn[i % keyInLength];
  }
}

/*******************************************************************************
* Description    : Decrypt memory block for USB interface
* Input          : buff - data to decrypt
*                  key - decryption key
*                  size - size of the buffer.
* Output         : buff - decrypted data.
* Return         : None.
*******************************************************************************/
void decryptMemory(BYTE *buff, const char *key, const uint32_t size) {
  cipherXOR(buff, key, size);
}

/*******************************************************************************
* Description    : Encrypt memory block for USB interface
* Input          : buff - data to encrypt
*                  key - encryption key
*                  size - size of the buffer.
* Output         : buff - encrypted data.
* Return         : None.
*******************************************************************************/
void encryptMemory(BYTE *buff, const char *key, const uint32_t size) {
  cipherXOR(buff, key, size);
}

/*******************************************************************************
* Description    : XOR cipher
* Input          : buff - data to encrypt/decrypt
*                  key - encryption/decryption key
*                  size - size of the buffer.
* Output         : buff - encrypted/decrypted data.
* Return         : None.
*******************************************************************************/
void cipherXOR(BYTE *buff, const char *key, const uint32_t size) {
  uint16_t keyLength = strlen(key) / 4;
  for (uint32_t i = 0; i < size / 4; ++i) {
      ((int32_t*) buff)[i] ^= ((int32_t *) key)[i % keyLength];
  }
}

/*******************************************************************************
* Description    : Resets timer for the command file scanning
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void resetTimerInerrupt(void) {
  htim14.Instance->CNT = 0;
}
#endif /* _USE_IOCTL == 1 */

