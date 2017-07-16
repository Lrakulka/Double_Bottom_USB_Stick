/* This file overrides default middleware loggic in the file sd_diskio.c */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "ff_gen_drv.h"
#include "sd_io_controller.h"
#include "usbd_storage_if.h"

/* Private typedef -----------------------------------------------------------*/
// Partition encryption
typedef enum {
	ENCRYPTED = 0,		
	DECRYPTED
} EncryptionType;

typedef struct {
   DWORD startSector;
	 DWORD lastSector;
	 UINT sectorNumber;
	 EncryptionType encryptionType;
} Partition;

typedef struct {
   Partition partitions[255];
	 uint8_t partitionsNumber;
	 uint8_t currPartitionNumber;
} PartitionsStructure;

/* Private define ------------------------------------------------------------*/
/* Block Size in Bytes */
#define BLOCK_SIZE                			 512
#define STORAGE_LUN_NBR                  0  

/* Disk Status Bits (DSTATUS) */
#define STA_INIT		  0x00	/* Drive initialized */
#define STA_NOINIT		0x01	/* Drive not initialized */
#define STA_NODISK		0x02	/* No medium in the drive */
#define STA_PROTECT		0x04	/* Write protected */

/* Private variables ---------------------------------------------------------*/
static volatile PartitionsStructure partitionsStructure;
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

extern HAL_SD_CardInfoTypedef SDCardInfo;

/* Private controller function prototypes -----------------------------------------------*/
// Operations with current partition
DWORD getPartitionSector(DWORD);
uint8_t isPartitionContainsMemory(DWORD, UINT);
Partition getPartition(void);
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
DSTATUS SD_initialize(BYTE lun)
{
	if ((Stat == STA_INIT) || (BSP_SD_Init() == MSD_OK))
	{
    Stat = STA_INIT;
	}
	
  return Stat;
}

/**
  * @brief  Gets Disk Status
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_status(BYTE lun)
{
  Stat = STA_INIT;

  if(BSP_SD_GetStatus() != MSD_OK)
  {
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
  DRESULT res = RES_OK;
  
	DWORD shiftedSector = getPartitionSector(sector);
	if (isPartitionContainsMemory(shiftedSector, count) 
		|| (BSP_SD_ReadBlocks_DMA((uint32_t*) buff, (uint64_t) (shiftedSector * BLOCK_SIZE), BLOCK_SIZE, count) != MSD_OK)) {
		res = RES_ERROR;
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
DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_OK;
	
	DWORD shiftedSector = getPartitionSector(sector);
  if (isPartitionContainsMemory(shiftedSector, count)
		|| (BSP_SD_WriteBlocks_DMA((uint32_t*) buff, (uint64_t) (shiftedSector * BLOCK_SIZE), BLOCK_SIZE, count) != MSD_OK)) {
		res = RES_ERROR;
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
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
  DRESULT res = RES_ERROR;
  SD_CardInfo CardInfo;
  
  if (Stat & STA_NOINIT) return RES_NOTRDY;
  
  switch (cmd)
  {
  /* Make sure that no pending write process */
  case CTRL_SYNC :
    res = RES_OK;
    break;
  
  /* Get number of sectors on the disk (DWORD) */
  case GET_SECTOR_COUNT :
    BSP_SD_GetCardInfo(&CardInfo);
    *(DWORD*)buff = CardInfo.CardCapacity / BLOCK_SIZE;
    res = RES_OK;
    break;
  
  /* Get R/W sector size (WORD) */
  case GET_SECTOR_SIZE :
    *(WORD*)buff = BLOCK_SIZE;
    res = RES_OK;
    break;
  
  /* Get erase block size in unit of sector (DWORD) */
  case GET_BLOCK_SIZE :
    *(DWORD*)buff = BLOCK_SIZE;
    break;
  
  default:
    res = RES_PARERR;
  }
  
  return res;
}

/* USB interface logic ---------------------------------------------------------*/
int8_t currentPartitionCapacity(uint32_t *block_num, uint16_t *block_size) {
	*block_num = getPartition().sectorNumber;
	*block_size = BLOCK_SIZE;
	return USBD_OK;
}

int8_t currentPartitionRead(BYTE *buff, DWORD sector, UINT count) {
	return SD_read(STORAGE_LUN_NBR, buff, sector, count);
}
int8_t currentPartitionWrite(BYTE *buff, DWORD sector, UINT count) {
	return SD_write(STORAGE_LUN_NBR, buff, sector, count);
}

int8_t currentPartitionInit(void) {
	return USBD_OK;
}

int8_t currentPartitionisReady(void) {
	return USBD_OK;
}

int8_t currentPartitionIsWriteProtected(void) {
	return USBD_OK;
}
	
int8_t currentPartitionMaxLun(void) {
	return STORAGE_LUN_NBR;
}

// Controller partition logic
DSTATUS initControllerMemory(void) {
	DSTATUS res = RES_ERROR;
	if (SD_initialize(STORAGE_LUN_NBR) == RES_OK) {
		// TODO: Write data getter from last SD Card sector
		partitionsStructure.partitionsNumber = 2;
		partitionsStructure.partitions[0].startSector = 0x0;
		partitionsStructure.partitions[0].lastSector = (SDCardInfo.CardCapacity / SDCardInfo.CardBlockSize) / 2;
		partitionsStructure.partitions[0].sectorNumber = partitionsStructure.partitions[0].lastSector;
		
		partitionsStructure.partitions[1].startSector = partitionsStructure.partitions[0].lastSector + 1;
		partitionsStructure.partitions[1].lastSector = SDCardInfo.CardCapacity / SDCardInfo.CardBlockSize;
		partitionsStructure.partitions[1].sectorNumber = partitionsStructure.partitions[1].lastSector - partitionsStructure.partitions[1].startSector;
		partitionsStructure.currPartitionNumber = 1;
		
		res = RES_OK;
	}
	return res;
}

/* Private controller functions ---------------------------------------------------------*/
DWORD getPartitionSector(DWORD sector) {
	return sector + getPartition().startSector;
}

uint8_t isPartitionContainsMemory(DWORD shiftedSector, UINT count) {
	return shiftedSector + count > getPartition().lastSector ? 1: 0;
}

Partition getPartition() {
	return partitionsStructure.partitions[partitionsStructure.currPartitionNumber];
}
#endif /* _USE_IOCTL == 1 */

