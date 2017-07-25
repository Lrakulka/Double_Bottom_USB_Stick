#include <string.h>
#include <stdlib.h>
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

int8_t isNewLineOrEnd(const char*, const uint32_t*, const uint32_t*);
void formConfFileText(FIL*, const PartitionsStructure*);
uint8_t parseConfStructure(PartitionsStructure*);
uint8_t isNewCommandFile(const FILINFO*, const char*, const WORD*);
Command getCommand(char*);
uint8_t scrollToLineEnd(const char*, const uint32_t*, uint32_t*);
uint8_t findWordBeforeSpace(const char*, const uint32_t*, uint32_t*, uint8_t*);
void formConfFileText(FIL*, const PartitionsStructure*);
/* Public user intarface functions ---------------------------------------------------------*/
uint8_t initFatFsForPartiton() {
	uint8_t res = 1;
	if(f_mount(&SDFatFs, (TCHAR const*)SD_Path, 0) != FR_OK){    
		res = 0;      
	}
	return res;
}

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
				if (isNewCommandFile(&fno, COMMAND_FILE_NAME, &commandFileLastModifDate) == 0) {
					commandFileLastModifDate = fno.ftime;
					commandExecutor();
				} 
			}
		}
		f_closedir(&dir);
	}
}

/* Private controller functions ---------------------------------------------------------*/
void commandExecutor(void) {
	FIL commandFile;																			/* File object */
	char buff[1000];
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
					if ((strcmp(partitionsStructure.confKey, password) == 0) 
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
					;
					// do nothing
				}
			}
		}		
	}
	f_close(&commandFile);
}

Command getCommand(char *command) {
	for (uint8_t i = 0;  i < sizeof (conversion) / sizeof (conversion[0]); ++i) {
		if (strcmp(command, conversion[i].str) == 0) {
				 return conversion[i].command; 
		}
	}
	return NO_COMMAND; 
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

uint8_t doRootConfig(const char *buff, const uint32_t *bytesRead, const uint32_t *shift) {
	uint8_t res = parseRootConfig(buff, bytesRead, shift, &newConfStructure);
	if (res == 0) {
		res = setConf(&newConfStructure, &partitionsStructure);
		if (res == 0) {
			res = changePartition(partitionsStructure.partitions[0].name, partitionsStructure.partitions[0].key);
			if (res == 0) {
				res = initFatFsForPartiton();
			}
		}
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
		if (res == 0) {
			res = initFatFsForPartiton();
		}
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

uint8_t scrollToLineEnd(const char *buff, const uint32_t *bytesRead, uint32_t *shift) {
	uint8_t res = 1;
	int8_t newLineStat;
	for (uint32_t i = *shift; i < *bytesRead; ++i) {
		newLineStat = isNewLineOrEnd(buff, &i, bytesRead);
		if (newLineStat != -1) {
			*shift = i + newLineStat + 1;
			res = 0;
			break;
		}
	}
	return res;
}

uint8_t findWordBeforeSpace(const char *buff, const uint32_t *bytesRead,
													uint32_t *start, uint8_t *size) {
	uint8_t res = 1;
	for (uint32_t i = *start; i < *bytesRead; ++i) {
		if ((buff[i] != ' ') && (buff[i] != '\t')) {
			*start = i;
			for (; i < *bytesRead; ++i) {
				if ((buff[i] == ' ') || (buff[i] == '\t')) {
					*size = i - *start;
					res = 0;
					break;
				}
			}
			break;
		}
	}
	return res;
}

uint8_t parseRootConfig(const char *buff, const uint32_t *bytesRead, 
												const uint32_t *shift, PartitionsStructure *newPartitionsStructure) {
	uint8_t res = 1;
	uint32_t start = *shift;
  uint8_t size;
	char *end;
	char buffer[10];
	// Set sequence
	strcpy(newPartitionsStructure->checkSequence, CHECK_SEQUENCE);									
	// Get password for configuration
	if (findWordBeforeSpace(buff, bytesRead, &start, &size) == 0) {
		strncpy(newPartitionsStructure->confKey, buff + start, size);		
		start += size;
		if (scrollToLineEnd(buff, bytesRead, &start) == 0) {
			// Get root password
			if (findWordBeforeSpace(buff, bytesRead, &start, &size) == 0) {
				strncpy(newPartitionsStructure->rootKey, buff + start, size);		
				start += size;
				// Miss one line
				if ((scrollToLineEnd(buff, bytesRead, &start) == 0) 
					&& (scrollToLineEnd(buff, bytesRead, &start) == 0)) {
					// Get partition configuration
					uint8_t i = 0;
					uint8_t partNumber = 0;
					for (uint8_t part; i < MAX_PART_NUMBER; ++i) {
						// Get partition number
						if (findWordBeforeSpace(buff, bytesRead, &start, &size) != 0) {
							break;
						}
						memset(buffer, '\0', sizeof(buffer));
						strncpy(buffer, buff + start, size);		
						start += size;
						part = strtol(buffer, &end, 10); 
						if (end == buffer || *end != '\0') {
							break;
						}
						// Get partition name
						if (findWordBeforeSpace(buff, bytesRead, &start, &size) != 0) {
							break;
						}
						strncpy(newPartitionsStructure->partitions[part].name, buff + start, size);		
						start += size;
						// Get partition key
						if (findWordBeforeSpace(buff, bytesRead, &start, &size) != 0) {
							break;
						}
						strncpy(newPartitionsStructure->partitions[part].key, buff + start, size);		
						start += size;
						// Set partition type (encrypted ot not)
						if ((part == 0) 
								|| (strcmp(newPartitionsStructure->partitions[part].key, PUBLIC_PARTITION_KEY) == 0)) {
							newPartitionsStructure->partitions[part].encryptionType = NOT_ENCRYPTED;
						} else {
							newPartitionsStructure->partitions[part].encryptionType = ENCRYPTED;
						}
						// Get partition number of sectors
						if (findWordBeforeSpace(buff, bytesRead, &start, &size) != 0) {
							break;
						}
						if (part != 0) {
							newPartitionsStructure->partitions[part]
									.startSector = newPartitionsStructure->partitions[part - 1].lastSector + 1;
						} else {
							newPartitionsStructure->partitions[part].startSector = 0;
						}
						memset(buffer, '\0', sizeof(buffer));
						strncpy(buffer, buff + start, size);
						start += size;
						newPartitionsStructure->partitions[part].sectorNumber = strtol(buffer, &end, 10); 
						if (end == buffer || *end != '\0') {
							break;
						}
						newPartitionsStructure->partitions[part].lastSector = newPartitionsStructure->partitions[part].startSector 
								+ newPartitionsStructure->partitions[part].sectorNumber;
						if (scrollToLineEnd(buff, bytesRead, &start) != 0) {
							break;
						}
						partNumber++;
					}
					if (partNumber > 1) {
						newPartitionsStructure->partitionsNumber = partNumber;
						res = 0;
					}
				}
			}
		}
	}
							
	return res;
}

			
uint8_t doShowConfig(const char *fileName, const PartitionsStructure *partitionsStructure) {
	FIL configFile;    
	uint8_t res = 1;
	
	res = f_open(&configFile, fileName, FA_CREATE_ALWAYS | FA_WRITE);
	if (res == FR_OK) {
		formConfFileText(&configFile, partitionsStructure);
		f_close(&configFile);
	}
	return res;
}

void formConfFileText(FIL *fil, const PartitionsStructure *partitionsStructure) {
	f_printf(fil, "%s\n", "-------Configurations of The Device---->To save changes please delete this line");
	f_printf(fil, "%s\n", conversion[1].str);			// Update to update configurations
	f_printf(fil, "%s\n", partitionsStructure->confKey);
	// Configuration
	f_printf(fil, "%s <--- Key for revealing device configurations\n", partitionsStructure->confKey);
	f_printf(fil, "%s <--- Root Key of the device\n", partitionsStructure->rootKey);
	// Partitons table
	f_printf(fil, "#N___________Name___________Key___________Number of sectors\n");
	f_printf(fil, "%-3s", "0"); 
	f_printf(fil, "%-20s ", partitionsStructure->partitions[0].name); 
	f_printf(fil, "%-20s ", PUBLIC_PARTITION_KEY);
	f_printf(fil, "%-10d\n", partitionsStructure->partitions[0].sectorNumber);
	for (uint8_t i = 1; i < partitionsStructure->partitionsNumber; ++i) {
		f_printf(fil, "%-3d", i);
		f_printf(fil, "%-20s ", partitionsStructure->partitions[i].name);
		f_printf(fil, "%-20s ", partitionsStructure->partitions[i].key);
		f_printf(fil, "%-10d\n", partitionsStructure->partitions[i].sectorNumber);
	}
	f_printf(fil, "-------------SD card available memory-------------\n");
	f_printf(fil, "%-15u     <- Card capacity memory\t\n", SDCardInfo.CardCapacity); 
	f_printf(fil, "%-15u     <- Card block size\t\n", SDCardInfo.CardBlockSize);
	f_printf(fil, "%-15u     <- Card block sector number\t\n", 
						SDCardInfo.CardCapacity / SDCardInfo.CardBlockSize - STORAGE_SECTOR_SIZE);
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

uint8_t isNewCommandFile(const FILINFO *fno, const char *fileName, const WORD *lastModifTime) {
	return ((strcmp(fno->fname, fileName) == 0) && (fno->ftime != *lastModifTime)) ? 0 : 1;
}
