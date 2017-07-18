#include <string.h>
#include "user_interface.h"
#include "fatfs.h"
#include "sd_io_controller.h"

#define ROOT_CONFIG_FILE_NAME 					"ROOTCON_.TXT"
#define ROOT_CONFIG_FILE_NAME_FAILED 		"ROOTCONF.TXT"
#define PART_CONFIG_FILE_NAME 					"PARTCON_.TXT"
#define PART_CONFIG_FILE_NAME_FAILED 		"PARTCONF.TXT"

/* Private varibles -----------------------------------------------*/
FATFS SDFatFs;  										/* File system object for SD card logical drive */
FIL configFile;											/* File object */
char buff[256];
uint32_t bytesWritten, bytesRead; 	/* File write/read counts */
WORD rootConfigLastModifDate = 0;		/* Last modified date of root config file*/
WORD partConfigLastModifDate = 0;		/* Last modified date of partition config file*/

/* Private user intarface function prototypes -----------------------------------------------*/
uint8_t isConfigFile(const FILINFO*, const char*, const WORD*);
FRESULT doRootConfig(void);
FRESULT doPartConfig(void);
/* Public user intarface functions ---------------------------------------------------------*/
void checkConfFiles() {
	FRESULT res;
	DIR dir;
	static FILINFO fno;
	
	res = f_opendir(&dir, SD_Path);                       			/* Open the directory */
	if (res == FR_OK) {
		for (;;) {
			res = f_readdir(&dir, &fno);                   					/* Read a directory item */
			if (res != FR_OK || fno.fname[0] == 0) break;  					/* Break on error or end of dir */
			if (!(fno.fattrib & AM_DIR)) {                    			/* It is a file */
				// Is root dir contains configurations files
				if (isConfigFile(&fno, ROOT_CONFIG_FILE_NAME, &rootConfigLastModifDate) == 0) {
					rootConfigLastModifDate = fno.ftime;
					doRootConfig();
				} else {
					if (isConfigFile(&fno, PART_CONFIG_FILE_NAME, &partConfigLastModifDate) == 0) {
						partConfigLastModifDate = fno.ftime;					
						doPartConfig();
					}
				}
			}
		}
		f_closedir(&dir);
	}
}

void initPartiton(const char* partName) {
	
	if(f_mount(&SDFatFs, (TCHAR const*)SD_Path, 0) != FR_OK){    
		/* FatFs Initialization Error : set the red LED on */       
	}
}
/* Private controller functions ---------------------------------------------------------*/
FRESULT doRootConfig() {
	/*if ((checkConfigModifDate(ROOT_CONFIG_FILE_NAME, &rootConfigLastModifDate) == FR_OK)
		&& (f_open(&configFile, ROOT_CONFIG_FILE_NAME, FA_READ) == FR_OK) 
		&& (f_read(&configFile, buff, sizeof(buff), &bytesRead) == FR_OK)) {

			if (doRootConfig(buff, &bytesRead) == FR_OK) {
				f_close(&configFile);
				f_unlink(ROOT_CONFIG_FILE_NAME);
			} else {
				f_close(&configFile);
				//f_rename(ROOT_CONFIG_FILE_NAME, ROOT_CONFIG_FILE_NAME_FAILED);
			}				
	}*/

	return FR_OK;
}

FRESULT doPartConfig() {
	/*
	if ((checkConfigModifDate(PART_CONFIG_FILE_NAME, &partConfigLastModifDate) == FR_OK)
		&& (f_open(&configFile, PART_CONFIG_FILE_NAME, FA_READ) == FR_OK) 
		&& (f_read(&configFile, buff, sizeof(buff), &bytesRead) == FR_OK)) {
			char *rootKey;
			char *partName;
			char *partKey;
			if (doPartConfig(buff, &bytesRead, rootKey, partName, partKey) == FR_OK) {
				f_close(&configFile);
				f_unlink(PART_CONFIG_FILE_NAME);
			} else {
				f_close(&configFile);
				//f_rename(PART_CONFIG_FILE_NAME, PART_CONFIG_FILE_NAME_FAILED);
			}		
	}
		
	FRESULT res = FR_INVALID_PARAMETER;
	for (uint32_t i = 0; i < *bytesRead; ++i) {
		if (buff[i] == '\n') {
			strncpy(rootKey, buff, i);
			for (uint32_t j = i + 1; j < *bytesRead; ++j) {
				if (buff[i] == ' ') {
					strncpy(partName, buff + i + 1, j - i - 1);
					strncpy(partKey, buff + j + 1, *bytesRead - j - 1);
					res = FR_OK;
				}
			}
		}
	}*/
	return FR_OK;
}

uint8_t isConfigFile(const FILINFO *fno, const char *fileName, const WORD *lastModifTime) {
	return ((strcmp(fno->fname, fileName) == 0) && (fno->ftime != *lastModifTime)) ? 0 : 1;
}
