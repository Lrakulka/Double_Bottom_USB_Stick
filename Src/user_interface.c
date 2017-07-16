#include "user_interface.h"
#include "fatfs.h"
#include "sd_io_controller.h"

#define ROOT_CONFIG_FILE_NAME "root_config.txt"
#define ROOT_CONFIG_FILE_NAME_FAILED "root_config_failed.txt"
#define PART_CONFIG_FILE_NAME "part_config.txt"
#define PART_CONFIG_FILE_NAME_FAILED "part_config_failed.txt"

/* Private varibles -----------------------------------------------*/
FATFS SDFatFs;  										/* File system object for SD card logical drive */
FIL configFile;											/* File object */
char buff[256];
uint32_t bytesWritten, bytesRead; 	/* File write/read counts */
WORD rootConfigLastModifDate = 0;		/* Last modified date of root config file*/
WORD partConfigLastModifDate = 0;		/* Last modified date of partition config file*/

/* Private user intarface function prototypes -----------------------------------------------*/
FRESULT checkConfigModifDate(const char*, WORD*);
FRESULT doRootConfig(const char*, const uint32_t*);
FRESULT doPartConfig(const char*, const uint32_t*);
/* Public user intarface functions ---------------------------------------------------------*/
void checkConfFiles() {
	if ((checkConfigModifDate(ROOT_CONFIG_FILE_NAME, &rootConfigLastModifDate) == FR_OK)
		&& (f_open(&configFile, ROOT_CONFIG_FILE_NAME, FA_READ) == FR_OK) 
		&& (f_read(&configFile, buff, sizeof(buff), &bytesRead) == FR_OK)) {

			if (doRootConfig(buff, &bytesRead) == FR_OK) {
				f_close(&configFile);
				f_unlink(ROOT_CONFIG_FILE_NAME);
			} else {
				f_close(&configFile);
				//f_rename(ROOT_CONFIG_FILE_NAME, ROOT_CONFIG_FILE_NAME_FAILED);
			}				
	}
	if ((checkConfigModifDate(PART_CONFIG_FILE_NAME, &partConfigLastModifDate) == FR_OK)
		&& (f_open(&configFile, PART_CONFIG_FILE_NAME, FA_READ) == FR_OK) 
		&& (f_read(&configFile, buff, sizeof(buff), &bytesRead) == FR_OK)) {
			
			if (doPartConfig(buff, &bytesRead) == FR_OK) {
				f_close(&configFile);
				f_unlink(PART_CONFIG_FILE_NAME);
			} else {
				f_close(&configFile);
				//f_rename(PART_CONFIG_FILE_NAME, PART_CONFIG_FILE_NAME_FAILED);
			}		
	}
}

void initPartiton(const char* partName) {
	
	if(f_mount(&SDFatFs, (TCHAR const*)SD_Path, 0) != FR_OK){    
		/* FatFs Initialization Error : set the red LED on */        
		HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_RESET);         
		while(1);      
	}
}
/* Private controller functions ---------------------------------------------------------*/
FRESULT doRootConfig(const char *buff, const uint32_t *bytesRead) {
	//TODO
	return FR_OK;
}

FRESULT doPartConfig(const char *buff, const uint32_t *bytesRead) {
	//TODO
	return FR_OK;
}

FRESULT checkConfigModifDate(const char *path, WORD *lastModifTime) {
	FRESULT res;
	DIR dir;
	static FILINFO fno;

	res = f_opendir(&dir, path);                     /* Open the directory */
	if (res == FR_OK) {
		res = f_readdir(&dir, &fno);                   /* Read a directory item */
		if (res != FR_OK || fno.fname[0] == 0) {
			res = FR_INT_ERR;  												   /* Break on error or end of dir */
		} else {
			if ((fno.fattrib & AM_DIR)								   /* It is a directory */
				 || (*lastModifTime == fno.ftime)) {        
				res = FR_INVALID_NAME;
			} else {                                     /* It is a file. */
				res = FR_OK;
			}
		}
		f_closedir(&dir);
	}
	return res;
}
