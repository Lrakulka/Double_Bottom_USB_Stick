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

#define ECB 														 1							// Enable both ECB
																												// Define length of AES encryption key
#if defined AES128
	#define AES_KEY_SIZE									 16
#elif defined AES192
	#define AES_KEY_SIZE									 24
#elif defined AES256
	#define AES_KEY_SIZE									 32
#else
	#define AES_KEY_SIZE									 -1
#endif

/* Disk Status Bits (DSTATUS) */
#define STA_INIT      0x00  /* Drive initialized */
#define STA_NOINIT    0x01  /* Drive not initialized */
#define STA_NODISK    0x02  /* No medium in the drive */
#define STA_PROTECT   0x04  /* Write protected */

/* Private variables ---------------------------------------------------------*/
PartitionsStructure partitionsStructure;									// Contains current device configurations
char longPartXORkey[STORAGE_BLOCK_SIZE];									// The password key for XOR cipher
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

extern HAL_SD_CardInfoTypedef SDCardInfo;
// Timer interrupt for command interrupt search
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
  	if (getPartition()->encryptionType == ENCRYPTED) {
  		decryptMemory(buff, longPartXORkey, count * SDCardInfo.CardBlockSize);
  	}
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
  if (getPartition()->encryptionType == ENCRYPTED) {
  	encryptMemory((BYTE*) buff, longPartXORkey, count * SDCardInfo.CardBlockSize);
  }
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
/* Return capacity of currently visible partition */
int8_t currentPartitionCapacity(uint32_t *block_num, uint16_t *block_size) {
	resetTimerInerrupt();
  *block_size = STORAGE_BLOCK_SIZE;
  if (partitionsStructure.initializeStatus == INITIALIZED) {
  	*block_num = getPartition()->sectorNumber;
  } else {																														// If the configurations not initialized
  	 FATFS *fs;
  	 DWORD fre_clust;
  	 if (f_getfree((TCHAR const*)SD_Path, &fre_clust, &fs) == FR_OK) {
  		 *block_num = (fs->n_fatent - 2) * fs->csize;				// Trying to get capacity by FatFs
  		 getPartition()->sectorNumber = *block_num;
  		 getPartition()->lastSector = *block_num - 1;
  	 } else {
  		 *block_num = SDCardInfo.CardCapacity / STORAGE_BLOCK_SIZE - 1;// Get capacity of SD card
  	 }
  }
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

/* Switch current visible partition to another */
uint8_t changePartition(const char *partName, const char *partKey) {
  uint8_t res = 1;
  for (uint8_t partNmb = 0; partNmb < partitionsStructure.partitionsNumber; ++partNmb) {
    if ((strncmp(partName, partitionsStructure.partitions[partNmb].name, PART_NAME_LENGHT) == 0)
    		&& ((partitionsStructure.partitions[partNmb].encryptionType == NOT_ENCRYPTED)
    				|| (strncmp(partKey, partitionsStructure.partitions[partNmb].key, PART_KEY_LENGHT) == 0))) {
      partitionsStructure.currPartitionNumber = partNmb;
      createKeyWithSpecLength(getPartition()->key, longPartXORkey, STORAGE_BLOCK_SIZE);
      res = 0;
      break;
    }
  }
  return res;
}

/* Set new configurations to the device */
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
/* Save configuration to the storage */
uint8_t saveConf(const PartitionsStructure *partitionsStructure) {
  uint8_t res = 1;
  uint32_t memorySize = STORAGE_BLOCK_SIZE * STORAGE_SECTOR_NUMBER;
  uint64_t storageAddr = SDCardInfo.CardCapacity - STORAGE_SECTOR_NUMBER * STORAGE_BLOCK_SIZE;
  BYTE alignMemory[memorySize];
  memcpy(alignMemory, partitionsStructure, sizeof(*partitionsStructure));
  encryptMemoryAES(alignMemory, partitionsStructure->rootKey, memorySize);
  
  if (BSP_SD_WriteBlocks_DMA((uint32_t*) alignMemory,
      storageAddr, STORAGE_BLOCK_SIZE, 
      STORAGE_SECTOR_NUMBER * SDCardInfo.CardBlockSize / STORAGE_BLOCK_SIZE) == MSD_OK) {
    res = 0;
  }
  return res;
}

/* Load the device configurations from the storage */
uint8_t loadConf(PartitionsStructure *partitionsStructure, const char *rootKey) {
	PartitionsStructure newConfStructure;
  uint8_t res = 1;
  uint32_t memorySize = STORAGE_BLOCK_SIZE * STORAGE_SECTOR_NUMBER;
  uint64_t storageAddr = SDCardInfo.CardCapacity - STORAGE_SECTOR_NUMBER * STORAGE_BLOCK_SIZE;
  BYTE alignMemory[memorySize];
  
  if (BSP_SD_ReadBlocks_DMA((uint32_t*) alignMemory, storageAddr, 
      STORAGE_BLOCK_SIZE, STORAGE_SECTOR_NUMBER * SDCardInfo.CardBlockSize / STORAGE_BLOCK_SIZE) == MSD_OK) {
  	decryptMemoryAES(alignMemory, rootKey, memorySize);
    memcpy((void*) &newConfStructure, alignMemory, sizeof(newConfStructure));
    // Check data correctness
    if (strncmp(newConfStructure.rootKey, rootKey, ROOT_KEY_LENGHT) == 0) {
      res = 0;
      *partitionsStructure = newConfStructure;
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

/* Init starting configurations for device */
uint8_t initStartConf() {
	partitionsStructure.partitionsNumber = 2;
	partitionsStructure.currPartitionNumber = 0;
	strcpy(partitionsStructure.partitions[0].name, "part0");
	partitionsStructure.partitions[0].startSector = 0x0;
	partitionsStructure.partitions[0].lastSector = (SDCardInfo.CardCapacity / STORAGE_BLOCK_SIZE) / 2;
	partitionsStructure.partitions[0].sectorNumber = partitionsStructure.partitions[0].lastSector + 1;
	partitionsStructure.partitions[0].encryptionType = NOT_ENCRYPTED;

	memset(partitionsStructure.partitions[1].name, '\0', sizeof(partitionsStructure.partitions[1].name));
	memset(partitionsStructure.partitions[1].key, '\0', sizeof(partitionsStructure.partitions[1].key));
	strcpy(partitionsStructure.partitions[1].name, "part1");
	strcpy(partitionsStructure.partitions[1].key, "part1Key");
	partitionsStructure.partitions[1].startSector = partitionsStructure.partitions[0].lastSector + 1;
	partitionsStructure.partitions[1].lastSector = SDCardInfo.CardCapacity / STORAGE_BLOCK_SIZE
			- STORAGE_SECTOR_NUMBER - 1;
	partitionsStructure.partitions[1].sectorNumber = partitionsStructure.partitions[1].lastSector
			- partitionsStructure.partitions[1].startSector + 1;
	partitionsStructure.partitions[1].encryptionType = ENCRYPTED;

	strcpy(partitionsStructure.confKey, "confKey");
	strcpy(partitionsStructure.rootKey, "rootKey");

	partitionsStructure.initializeStatus = INITIALIZED;
	return saveConf(&partitionsStructure);
}

/* Return sector with respect of current visible partition */
DWORD getPartitionSector(DWORD sector) {
  return sector + getPartition()->startSector;
}

/* Return true if requested sector contains in current visible partition*/
uint8_t isPartitionContainsMemorySectors(DWORD shiftedSector, UINT count) {
	shiftedSector += count;
  return (shiftedSector >= getPartition()->startSector) && (shiftedSector <= getPartition()->lastSector)
  		? 1
  		: 0;
}

/* Returns current visible partition configurations */
Partition* getPartition() {
  return &partitionsStructure.partitions[partitionsStructure.currPartitionNumber];
}

/* Decrypt memory block */
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

/* Encrypt memory block */
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

/* Decrypt memory block */
void decryptMemory(BYTE *buff, const char *key, const uint32_t size) {
	cipherXOR(buff, key, size);
}

/* Encrypt memory block */
void encryptMemory(BYTE *buff, const char *key, const uint32_t size) {
	cipherXOR(buff, key, size);
}

void cipherXOR(BYTE *buff, const char *key, const uint32_t size) {
	uint16_t keyLength = strlen(key) / 4;
	for (uint32_t i = 0; i < size / 4; ++i) {
			((int32_t*) buff)[i] ^= ((int32_t *) key)[i % keyLength];
	}
}

/* Resets timer for the command file scanning */
void resetTimerInerrupt(void) {
	htim14.Instance->CNT = 0;
}
#endif /* _USE_IOCTL == 1 */

