#include <string.h>
#include "user_interface.h"
#include "fatfs.h"
#include "sd_io_controller.h"

#define COMMAND_FILE_NAME								"COMMAND_.TXT"
#define COMMAND_FILE_NAME_FAILED				"COMMANDF.TXT"

#define COMMAND_MAX_LENGTH							10					
// Partition encryption
typedef enum {
	CHANGE_PARTITION = 0,	
	UPDATE_ROOT_CONFIGURATIONS,
	SHOW_ROOT_CONFIGURATIONS,
	// Add new commands here
	NO_COMMAND
} Command;

const static struct {
    Command    command;
    const char *str;
} conversion [] = {
    {CHANGE_PARTITION, 						"ChangePart"},
    {UPDATE_ROOT_CONFIGURATIONS, 	"UpdateConf"},
    {SHOW_ROOT_CONFIGURATIONS, 		"ShowConf"}
};

/* Private varibles -----------------------------------------------*/
FATFS SDFatFs;  										/* File system object for SD card logical drive */
WORD commandFileLastModifDate = 0;		/* Last modified date of command file*/
extern HAL_SD_CardInfoTypedef SDCardInfo;
extern PartitionsStructure partitionsStructure;
PartitionsStructure newConfStructure;

/* Private user intarface function prototypes -----------------------------------------------*/
// Executes command
void commandExecutor(void);
// Command executors
uint8_t doRootConfig(const char*, const uint32_t*, const uint32_t*);
uint8_t doPartConfig(const char*, const uint32_t*, const uint32_t*);
uint8_t doShowConfig(const char*, const PartitionsStructure*);
// Parsers
uint8_t parsePartConfig(const char*, const uint32_t*, const uint32_t*, char*, char*);
uint8_t parseRootConfig(const char*, const uint32_t*, const uint32_t*, PartitionsStructure*);
void getCommandAndPassword(const char*, const uint32_t*, uint32_t*, Command*, char*);

Command getCommand(char*);
int8_t isNewLineOrEnd(const char*, const uint32_t*, const uint32_t*);
void formConfFileText(FIL*, const PartitionsStructure*);
uint8_t parseConfStructure(PartitionsStructure*);
uint8_t isNewRootFile(const FILINFO*, const char*, const WORD*);

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
				// Is root dir contains command file
				if (isNewRootFile(&fno, COMMAND_FILE_NAME, &commandFileLastModifDate) == 0) {
					commandFileLastModifDate = fno.ftime;
					commandExecutor();
				} 
			}
		}
		f_closedir(&dir);
	}
}

void initPartiton(const char* partName) {
	// TODO
	if(f_mount(&SDFatFs, (TCHAR const*)SD_Path, 0) != FR_OK){    
		/* FatFs Initialization Error : set the red LED on */       
	}
}

/* Private controller functions ---------------------------------------------------------*/
void commandExecutor(void) {
	FIL commandFile;																			/* File object */
	char buff[256];
	uint32_t bytesRead; 																	/* File write/read counts */
	Command command;
#if ROOT_KEY_LENGHT > CONF_KEY_LENGHT && ROOT_KEY_LENGHT > SHOW_CONF_KEY_LENGHT
	char password[ROOT_KEY_LENGHT];
	memset(password, '\0', ROOT_KEY_LENGHT);
#elif CONF_KEY_LENGHT > ROOT_KEY_LENGHT && CONF_KEY_LENGHT > SHOW_CONF_KEY_LENGHT
	char password[CONF_KEY_LENGHT];
	memset(password, '\0', CONF_KEY_LENGHT);
#else
	char password[SHOW_CONF_KEY_LENGHT];
	memset(password, '\0', SHOW_CONF_KEY_LENGHT);
#endif
	uint32_t shiftPosition;
	
	if ((f_open(&commandFile, COMMAND_FILE_NAME, FA_READ) == FR_OK) 
		&& (f_read(&commandFile, buff, sizeof(buff), &bytesRead) == FR_OK)) {
		if (bytesRead != 0) {
			getCommandAndPassword(buff, &bytesRead, &shiftPosition, &command, password);
			
			switch (command) {
				case SHOW_ROOT_CONFIGURATIONS: {
					if ((strcmp(partitionsStructure.confKey, password) == 0))
						f_close(&commandFile);
						if (doShowConfig("wsgrsrg.txt", &partitionsStructure) != 0) {
							// ----f_rename(COMMAND_FILE_NAME, COMMAND_FILE_NAME_FAILED);
						}
					break;
				}
				case UPDATE_ROOT_CONFIGURATIONS: {
					if ((strcmp(partitionsStructure.rootKey, password) == 0) 
							&& (doRootConfig(buff, &bytesRead, &shiftPosition) == 0)) {
						// ----f_unlink(PART_CONFIG_FILE_NAME);
					} else {
						// f_rename(COMMAND_FILE_NAME, COMMAND_FILE_NAME_FAILED);
					}
					break;
				}
				case CHANGE_PARTITION: {
					if ((strcmp(partitionsStructure.rootKey, password) == 0)
						&& (doPartConfig(buff, &bytesRead, &shiftPosition) == 0)) {
						// ----f_unlink(PART_CONFIG_FILE_NAME);
					} else {
						// f_rename(COMMAND_FILE_NAME, COMMAND_FILE_NAME_FAILED);
					}
					break;
				}
				default: {
					// do nothing
				}
			}
		}		
	}
	f_close(&commandFile);
}

void getCommandAndPassword(const char *buff, const uint32_t *bytesRead, 
														uint32_t *shift, Command *command, char *password) {
	int8_t shiftEndOfLine;
	char commandS[COMMAND_MAX_LENGTH];
	memset(commandS, '\0', COMMAND_MAX_LENGTH);
	
	for (uint32_t i = 0; i < *bytesRead; ++i) {
		shiftEndOfLine = isNewLineOrEnd(buff, &i, bytesRead);
		if (shiftEndOfLine != -1) {
			strncpy(commandS, buff, i);
			*shift = i + shiftEndOfLine + 1;
			for (uint32_t j = *shift; j < *bytesRead; ++j) {
				shiftEndOfLine = isNewLineOrEnd(buff, &j, bytesRead);
				if (shiftEndOfLine != -1) {
					strncpy(password, buff + *shift, j - *shift - shiftEndOfLine + 1);
					*shift = j + shiftEndOfLine + 1;
					break;
				}
			}
			break;
		}
	}
	*command = getCommand(commandS);
}

Command getCommand(char *command) {
	for (uint8_t i = 0;  i < sizeof (conversion) / sizeof (conversion[0]); ++i) {
		if (strcmp(command, conversion[i].str) == 0)
				 return conversion[i].command; 
	}
	return NO_COMMAND; 
}

uint8_t doRootConfig(const char *buff, const uint32_t *bytesRead, const uint32_t *shift) {
	uint8_t res =  parseRootConfig(buff, bytesRead, shift, &newConfStructure);
	if (res == 0) {
		res = setConf(&newConfStructure, &partitionsStructure);
	}
	return res;
}

uint8_t doPartConfig(const char *buff, const uint32_t *bytesRead, const uint32_t *shift) {
	char partName[PART_NAME_LENGHT];
	char partKey[PART_KEY_LENGHT];
	memset(partName, '\0', PART_NAME_LENGHT);
	memset(partKey, '\0', PART_KEY_LENGHT);
	uint8_t res = parsePartConfig(buff, bytesRead, shift, partName, partKey);
	if (res == 0) {
		res = changePartition(partName, partKey);
	}
	return res;
}

uint8_t parsePartConfig(const char *buff, const uint32_t *bytesRead, 
												const uint32_t *shift, char *partName, char *partKey) {
	uint8_t res = 1;
	
	for (uint32_t j = *shift; j < *bytesRead; ++j) {
		if (buff[j] == ' ') {
			strncpy(partName, buff + *shift, j - *shift);
			strncpy(partKey, buff + j + 1, *bytesRead - j - 1);
			res = 0;
			break;
		}
	}
	return res;
}

uint8_t parseRootConfig(const char *buff, const uint32_t *bytesRead, 
												const uint32_t *shift, PartitionsStructure *partitionsStructure) {
	uint8_t res = 1;
	
	return res;
}

			
uint8_t doShowConfig(const char *fileName, const PartitionsStructure *partitionsStructure) {
	FIL configFile;  
	uint32_t byteswritten;      
	uint8_t res = 1;
	
	res = f_open(&configFile, fileName, FA_CREATE_ALWAYS | FA_WRITE);
	if (res == FR_OK) {
		formConfFileText(&configFile, partitionsStructure);
		if (byteswritten != 0) {
			res = 0;			
		}
		f_close(&configFile);
	}
	return res;
}

void formConfFileText(FIL *fil, const PartitionsStructure *partitionsStructure) {
	f_printf(fil, "%s\r\n", "-------Configurations of The Device---->To save changes please delete this line");
	f_printf(fil, "%s\r\n", partitionsStructure->rootKey);
	// Configuration
	f_printf(fil, "%s <--- Key for revealing device configurations\r\n", partitionsStructure->confKey);
	f_printf(fil, "%s <--- Root Key of the device\r\n", partitionsStructure->rootKey);
	// Partitons table
	f_printf(fil, "#N___________Name___________Key___________Number of sectors\r\n");
	f_printf(fil, "%-3s:", "0"); 
	f_printf(fil, "%-20s ", partitionsStructure->partitions[0].name); 
	f_printf(fil, "%-20s ", "always public");
	f_printf(fil, "%-10d\r\n", partitionsStructure->partitions[0].sectorNumber);
	for (uint8_t i = 1; i < partitionsStructure->partitionsNumber; ++i) {
		f_printf(fil, "%-3d:", i);
		f_printf(fil, "%-20s ", partitionsStructure->partitions[i].name);
		f_printf(fil, "%-20s ", partitionsStructure->partitions[i].key);
		f_printf(fil, "%-10d\r\n", partitionsStructure->partitions[i].sectorNumber);
	}
	f_printf(fil, "-------------SD card available memory-------------\r\n");
	f_printf(fil, "%u     <- Card capacity memory\t\t\t\r\n", SDCardInfo.CardCapacity); 
	f_printf(fil, "%u     <- Card block size\t\t\t\r\n", SDCardInfo.CardBlockSize);
	f_printf(fil, "%u     <- Card block sector number\t\t\t\r\n", 
						SDCardInfo.CardCapacity / SDCardInfo.CardBlockSize - CONF_STORAGE_SIZE);
}


int8_t isNewLineOrEnd(const char *buff, const uint32_t *position, const uint32_t *size) {
	int8_t shift = -1;
	
	if ((buff[*position] == '\r') || (buff[*position] == '\n') || (*size == *position + 1)) {
		shift = 0;
	}
	if (buff[*position] == '\r' && buff[*position + 1] == '\n') {
		shift = 1;
	}
	return shift;	
}

uint8_t isNewRootFile(const FILINFO *fno, const char *fileName, const WORD *lastModifTime) {
	return ((strcmp(fno->fname, fileName) == 0) && (fno->ftime != *lastModifTime)) ? 0 : 1;
}