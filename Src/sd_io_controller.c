/* This file overrides default middleware loggic in the file sd_diskio.c */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "ff_gen_drv.h"
#include "sd_io_controller.h"
#include "usbd_storage_if.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
/* Block Size in Bytes */
#define STORAGE_BLOCK_SIZE               512
#define STORAGE_LUN_NBR                  0  

/* Disk Status Bits (DSTATUS) */
#define STA_INIT      0x00  /* Drive initialized */
#define STA_NOINIT    0x01  /* Drive not initialized */
#define STA_NODISK    0x02  /* No medium in the drive */
#define STA_PROTECT   0x04  /* Write protected */

/* Private variables ---------------------------------------------------------*/
PartitionsStructure partitionsStructure;
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

extern HAL_SD_CardInfoTypedef SDCardInfo;
// Timer interrupt for command interrupt search
extern TIM_HandleTypeDef htim14;

/* Private controller function prototypes -----------------------------------------------*/
// Operations with current partition
DWORD getPartitionSector(DWORD);
uint8_t isPartitionContainsMemorySectors(DWORD, UINT);
Partition getPartition(void);
BYTE* decryptMemory(BYTE*, const char*, const uint32_t);
BYTE* encryptMemory(BYTE*, const char*, const uint32_t);
uint8_t checkNewPartitionsStructure(const PartitionsStructure*);
uint8_t saveConf(const PartitionsStructure*);
uint8_t loadConf(PartitionsStructure*, const char*);
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
    buff = decryptMemory(buff, getPartition().key, STORAGE_BLOCK_SIZE);
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
  encryptMemory((BYTE*) buff, getPartition().key, STORAGE_BLOCK_SIZE);
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
    *(DWORD*)buff = getPartition().sectorNumber;
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
/* Return capacity of currently visible partition */
int8_t currentPartitionCapacity(uint32_t *block_num, uint16_t *block_size) {
	resetTimerInerrupt();
  *block_num = getPartition().sectorNumber;
  *block_size = STORAGE_BLOCK_SIZE;
  return USBD_OK;
}

/* Read data from the storage */
int8_t currentPartitionRead(BYTE *buff, DWORD sector, UINT count) {
	resetTimerInerrupt();   																// Reset Timer for the command file scan
  return SD_read(STORAGE_LUN_NBR, buff, sector, count);
}

/* Write data from the storage */
int8_t currentPartitionWrite(BYTE *buff, DWORD sector, UINT count) {
	resetTimerInerrupt();   																// Reset Timer for the command file scan
  return SD_write(STORAGE_LUN_NBR, buff, sector, count);
}

int8_t currentPartitionInit(void) {
	resetTimerInerrupt();   																// Reset Timer for the command file scan
  return USBD_OK;
}

int8_t currentPartitionisReady(void) {
	resetTimerInerrupt();   																// Reset Timer for the command file scan
  return USBD_OK;
}

int8_t currentPartitionIsWriteProtected(void) {
	resetTimerInerrupt();   																// Reset Timer for the command file scan
  return USBD_OK;
}
  
int8_t currentPartitionMaxLun(void) {
	resetTimerInerrupt();   																// Reset Timer for the command file scan
  return STORAGE_LUN_NBR;
}
//-------------------------------------------------------------------------------------------------
// Controller partition logic
/* Load current device configuration */
DSTATUS initControllerMemory(void) {
  DSTATUS res = RES_ERROR;
  if (SD_initialize(STORAGE_LUN_NBR) == RES_OK) {
    // TODO: Write data getter from last SD Card sector
    partitionsStructure.partitionsNumber = 2;
    strcpy(partitionsStructure.partitions[0].name, "part0");
    partitionsStructure.partitions[0].startSector = 0x0;
    partitionsStructure.partitions[0].lastSector = (SDCardInfo.CardCapacity / STORAGE_BLOCK_SIZE) / 2;
    partitionsStructure.partitions[0].sectorNumber = partitionsStructure.partitions[0].lastSector + 1;
    
    memset(partitionsStructure.partitions[1].name, '\0', sizeof(partitionsStructure.partitions[1].name));
    memset(partitionsStructure.partitions[1].key, '\0', sizeof(partitionsStructure.partitions[1].key));
    strcpy(partitionsStructure.partitions[1].name, "part1");
    strcpy(partitionsStructure.partitions[1].key, "partKey");
    partitionsStructure.partitions[1].startSector = partitionsStructure.partitions[0].lastSector + 1;
    partitionsStructure.partitions[1].lastSector = SDCardInfo.CardCapacity / STORAGE_BLOCK_SIZE
    		- STORAGE_SECTOR_NUMBER - 1;
    partitionsStructure.partitions[1].sectorNumber = partitionsStructure.partitions[1].lastSector
    		- partitionsStructure.partitions[1].startSector + 1;
    partitionsStructure.currPartitionNumber = 0;
    
    strcpy(partitionsStructure.confKey, "confKey");
    strcpy(partitionsStructure.rootKey, "rootKey");
    //-------
    res = RES_OK;
  }
  return res;
}

/* Switch current visible partition to another */
uint8_t changePartition(const char *partName, const char *partKey) {
  uint8_t res = 1;
  for (uint8_t partNmb = 0; partNmb < partitionsStructure.partitionsNumber; ++partNmb) {
    if (strcmp(partName, partitionsStructure.partitions[partNmb].name) == 0) {
      partitionsStructure.currPartitionNumber = partNmb;          
      res = 0;
      break;
    }
  }
  return res;
}

/* Set new configurations to the device */
uint8_t setConf(const PartitionsStructure *newConf, PartitionsStructure *oldConf) {
  uint8_t res = checkNewPartitionsStructure(newConf);
  if (res == 0) {
    *oldConf = *newConf;
    oldConf->isInitilized = INITIALIZED;
    res = saveConf(oldConf);
  }
  return res;
}

/* Private controller functions ---------------------------------------------------------*/
/* Save configuration to the storage */
uint8_t saveConf(const PartitionsStructure *partitionsStructure) {
  uint8_t res = 1;
  uint32_t memorySize = STORAGE_BLOCK_SIZE * STORAGE_SECTOR_NUMBER;
  uint64_t storageAddr = SDCardInfo.CardCapacity - STORAGE_SECTOR_NUMBER * STORAGE_BLOCK_SIZE;
  BYTE alignMemory[memorySize];
  memcpy(alignMemory, partitionsStructure, sizeof(*partitionsStructure));
  encryptMemory(alignMemory, partitionsStructure->rootKey, STORAGE_BLOCK_SIZE);
  
  if (BSP_SD_WriteBlocks_DMA((uint32_t*) alignMemory,
      storageAddr, STORAGE_BLOCK_SIZE, 
      STORAGE_SECTOR_NUMBER * SDCardInfo.CardBlockSize / STORAGE_BLOCK_SIZE) == MSD_OK) {
    res = 0;
  }
  return res;
}

/* Load the device configurations from the storage */
uint8_t loadConf(PartitionsStructure *partitionsStructure, const char *rootKey) {
  uint8_t res = 1;
  uint32_t memorySize = STORAGE_BLOCK_SIZE * STORAGE_SECTOR_NUMBER;
  uint64_t storageAddr = SDCardInfo.CardCapacity - STORAGE_SECTOR_NUMBER * STORAGE_BLOCK_SIZE;
  BYTE alignMemory[memorySize];
  
  if (BSP_SD_ReadBlocks_DMA((uint32_t*) alignMemory, storageAddr, 
      STORAGE_BLOCK_SIZE, STORAGE_SECTOR_NUMBER * SDCardInfo.CardBlockSize / STORAGE_BLOCK_SIZE) == MSD_OK) {
  	decryptMemory(alignMemory, rootKey, STORAGE_BLOCK_SIZE * STORAGE_LUN_NBR);
    memcpy((void*) partitionsStructure, alignMemory, sizeof(*partitionsStructure));
    // Check data correctness
    if (strcmp(partitionsStructure->rootKey, rootKey) == 0) {
      res = 0;
    }
  }
  return res;
}

/* Check the device new configurations for errors */
uint8_t checkNewPartitionsStructure(const PartitionsStructure *partitionStructure) {
  uint32_t blockUsed = 0;
  if ((partitionStructure->confKey[0] == '\0') 
      || (partitionStructure->rootKey[0] == '\0')) {
    return 1;
  }
  for (uint8_t i = 0; i < partitionStructure->partitionsNumber; ++i) {
    if ((partitionStructure->partitions[i].name[0] == '\0') 
      || (partitionStructure->partitions[i].key[0] == '\0')
      || ((partitionStructure->partitions[i].encryptionType == NOT_ENCRYPTED) 
          && (strcmp(partitionStructure->partitions[i].key, PUBLIC_PARTITION_KEY) != 0))
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

/* Init starting configurations for device */
uint8_t initStartConf(const char *deviceUniqueID) {
	uint8_t res = 1;
  // TODO:
  res = 0;
  return res;
}

/* Return sector with respect of current visible partition */
DWORD getPartitionSector(DWORD sector) {
  return sector + getPartition().startSector;
}

/* Return true if requested sector contains in current visible partition*/
uint8_t isPartitionContainsMemorySectors(DWORD shiftedSector, UINT count) {
	shiftedSector += count;
  return (shiftedSector >= getPartition().startSector) && (shiftedSector <= getPartition().lastSector)
  		? 1
  		: 0;
}

/* Returns current visible partition configurations */
Partition getPartition() {
  return partitionsStructure.partitions[partitionsStructure.currPartitionNumber];
}

/* Decrypt memory block */
BYTE* decryptMemory(BYTE *buff, const char *key, const uint32_t size) {
  return buff;
}

/* Encrypt memory block */
BYTE* encryptMemory(BYTE *buff, const char *key, const uint32_t size) {
  return buff;
}

/* Resets timer for the command file scanning */
void resetTimerInerrupt(void) {
	htim14.Instance->CNT = 0;
}
#endif /* _USE_IOCTL == 1 */

