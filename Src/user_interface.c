#include "user_interface.h"
#include <string.h>
#include <stdlib.h>
#include "sd_io_controller.h"
#include "usbd_core.h"

#define COMMAND_FILE_NAME               "COMMAND_.TXT"
#define COMMAND_FILE_NAME_FAILED        "COMMANDF.TXT"

#define DEVICE_CONFIGS					        "CONFIGS_.TXT"

#define COMMAND_MAX_LENGTH              10          

#define USB_REINIT_DELAY								2000
// Partition encryption
typedef enum {
  CHANGE_PARTITION = 0,  
  UPDATE_ROOT_CONFIGURATIONS,
  SHOW_ROOT_CONFIGURATIONS,
	INIT_DEVICE_CONFIGURATIONS,
  // Add new commands here
  NO_COMMAND
} Command;

const static struct {
    Command    command;
    const char *str;
} conversion [] = {
    {CHANGE_PARTITION,            "ChangePart"},
    {UPDATE_ROOT_CONFIGURATIONS,  "UpdateConf"},
    {SHOW_ROOT_CONFIGURATIONS,    "ShowConf"},
		{INIT_DEVICE_CONFIGURATIONS,  "InitConf"}
    // Add new commands here
};

/* Private variables -----------------------------------------------*/
FATFS SDFatFs;                        // File system object for SD card logical drive 
WORD commandFileLastModifTime = 0;    // Last modified time of command file
extern HAL_SD_CardInfoTypedef SDCardInfo;
extern PartitionsStructure partitionsStructure;
extern USBD_HandleTypeDef hUsbDeviceFS;

PartitionsStructure newConfStructure;	// Contains new configuration for the device

/* The partition should be scanned for containing command file. 
 * If you delete this check the infinity loop can be created by two commands files.
*/
uint8_t isPartitionScanned;        
/* Private user interface function prototypes -----------------------------------------------*/
// Executes command
void executeCommandFile(void);
// Command executors
uint8_t doRootConfig(const char*, const uint32_t*, const uint32_t*);
uint8_t doPartConfig(const char*, const uint32_t*, const uint32_t*);
uint8_t doShowConfig(const char*, const PartitionsStructure*);
// Parsers
uint8_t parsePartConfig(const char*, const uint32_t*, const uint32_t*, char*, char*);
uint8_t parseRootConfig(const char*, const uint32_t*, const uint32_t*, PartitionsStructure*);
void getCommand(const char*, const uint32_t*, uint32_t*, Command*);
void getRootPassword(const char*, const uint32_t*, uint32_t*, char*);

uint8_t initDeviceConf(void);
int8_t isNewLineOrEnd(const char*, const uint32_t*, const uint32_t*);
void formConfFileText(FIL*, const PartitionsStructure*);
uint8_t isCommandFileUpdated(const FILINFO*, const char*, const WORD*);
uint8_t scrollToLineEnd(const char*, const uint32_t*, uint32_t*);
uint8_t findWordBeforeSpace(const char*, const uint32_t*, uint32_t*, uint8_t*);
void formConfFileText(FIL*, const PartitionsStructure*);
void commandExecutionResult(uint8_t);
void getLine(const char*, const uint32_t*, uint32_t*, char*, uint8_t);
uint8_t changePartAndReinitUSB(const char*, const char*);
/* Public user interface functions ---------------------------------------------------------*/
/* Scan root directory of current visible partition for the command file
 * Command file - the file that contains commands to device and have name COMMAND_FILE_NAME
 */
void checkConfFiles() {
  FRESULT res;
  DIR dir;
  FILINFO fno;
  
  res = f_mount(&SDFatFs, (TCHAR const*)SD_Path, 0);    // Mount and remount file system
  if (res == FR_OK) {
    res = f_opendir(&dir, SD_Path);                     // Open the directory 
    if (res == FR_OK) {
      for (;;) {
        res = f_readdir(&dir, &fno);                    // Read a directory item 
        if ((res != FR_OK) || (fno.fname[0] == 0)) {                 
          break;                                        // Break on error or end of dir 
        }
        if (!(fno.fattrib & AM_DIR)) {                  // It is a file 
          																							// Check for the command file
          if (isCommandFileUpdated(&fno, COMMAND_FILE_NAME, &commandFileLastModifTime)) {
            commandFileLastModifTime = fno.ftime;				// Remember time when the command file was updated
            if (isPartitionScanned != 0) {							// First scan of root dir shouldn't executes commands
            	executeCommandFile();
              f_closedir(&dir);
            	return;
            }
          } 
        }
      }
      f_closedir(&dir);
    } 
  }  
  if (res == FR_OK) {
    isPartitionScanned = 1;                             // Partition scanned
  } else {
  	isPartitionScanned = 0;															// Try to reinit file system and try to scan again
  }
  
  //f_mount(0, (TCHAR const*)SD_Path, 0);                 // Unmount
}

/* Private controller functions ---------------------------------------------------------*/
/* Get command from the command file and executes it */
void executeCommandFile(void) {
  FIL commandFile;      																// File object
  char buff[1000];
  uint32_t bytesRead;   																// File write/read counts
  Command command;																			// Command that should be executed
  char password[ROOT_KEY_LENGHT];
  memset(password, '\0', ROOT_KEY_LENGHT);
  uint32_t shiftPosition = 0;														// Uses for the command file parsing
	uint8_t commandResult = 1;

  if ((f_open(&commandFile, COMMAND_FILE_NAME, FA_READ) == FR_OK) 
  		&& (f_read(&commandFile, buff, sizeof(buff), (UINT*) &bytesRead) == FR_OK)) {
    if (bytesRead != 0) {
    																										// Get command and root password from the command file
    	getCommand(buff, &bytesRead, &shiftPosition, &command);
    	getRootPassword(buff, &bytesRead, &shiftPosition, password);
      																									// Init or Reinit the device with previous settings
      if ((command == INIT_DEVICE_CONFIGURATIONS)
      		&& (strncmp(DEVICE_UNIQUE_ID, password, ROOT_KEY_LENGHT) == 0)) {
      	commandExecutionResult(initDeviceConf());
      	f_close(&commandFile);
      	return;
      }
      																									// Load configurations
      if ((partitionsStructure.initializeStatus == NOT_INITIALIZED)
      		&& (loadConf(&partitionsStructure, password) != 0)) {
      	f_close(&commandFile);
      	return;
      }
    																										// If pass not matches then stop method execution
      if (strncmp(partitionsStructure.rootKey, password, ROOT_KEY_LENGHT) != 0) {
      	f_close(&commandFile);
      	return;
      }
      switch (command) {																// Executor of the command
        case SHOW_ROOT_CONFIGURATIONS: {								// Shows current device configurations
        	getLine(buff, &bytesRead, &shiftPosition, password, sizeof(password));
					f_close(&commandFile);
					if (strncmp(partitionsStructure.confKey, password, CONF_KEY_LENGHT) == 0) {
							commandResult = doShowConfig(DEVICE_CONFIGS, &partitionsStructure);
					}
          break;
        }
        case UPDATE_ROOT_CONFIGURATIONS: {							// Updates the device configurations
        	getLine(buff, &bytesRead, &shiftPosition, password, sizeof(password));
        	if (strncmp(partitionsStructure.confKey, password, CONF_KEY_LENGHT) == 0) {
        		commandResult = doRootConfig(buff, &bytesRead, &shiftPosition);
        	}
          break;
        }
        case CHANGE_PARTITION: {												// Changes current visible partition
        	commandResult = doPartConfig(buff, &bytesRead, &shiftPosition);
          break;
        }
        default: {
          // do nothing
        }
      }
      f_close(&commandFile);
      commandExecutionResult(commandResult);
      return;
    }    
  }
  f_close(&commandFile);
}

/* Changed the visible partition and reinit USB connection */
uint8_t changePartAndReinitUSB(const char *partName, const char *partKey) {
	uint8_t res = 1;
	if (USBD_Stop(&hUsbDeviceFS) == USBD_OK) {
		res = changePartition(partName, partKey);
		if (res == 0) {
			// Init new partition and scan it
			isPartitionScanned = 0;
		}
		HAL_Delay(USB_REINIT_DELAY);            // Time delay for host to recognize detachment of the stick
		USBD_Start(&hUsbDeviceFS);
	}
	return res;
}

/* Init the device with beginning configurations */
uint8_t initDeviceConf() {
	initStartConf();
	return changePartAndReinitUSB(partitionsStructure.partitions[0].name, partitionsStructure.partitions[0].key);
}

/* Get command from the command file */
void getCommand(const char *buff, const uint32_t *bytesRead, uint32_t *shift, Command *command) {
  char commandS[COMMAND_MAX_LENGTH];
  memset(commandS, '\0', COMMAND_MAX_LENGTH);

  getLine(buff, bytesRead, shift, commandS, COMMAND_MAX_LENGTH);		// Get command line text
  																															// Get command status
  for (uint8_t i = 0;  i < sizeof (conversion) / sizeof (conversion[0]); ++i) {
    if (strncmp(commandS, conversion[i].str, COMMAND_MAX_LENGTH) == 0) {
    	*command = conversion[i].command;
    	return;
    }
  }
  *command = NO_COMMAND;
}

/* Get root password from the command file */
void getRootPassword(const char *buff, const uint32_t *bytesRead, uint32_t *shift, char *password) {
  getLine(buff, bytesRead, shift, password, ROOT_KEY_LENGHT);			// Get password line text
}

void getLine(const char *buff, const uint32_t *bytesRead, uint32_t *shift,
						 char *line, uint8_t lineMaxSize) {
  int8_t shiftEndOfLine;
  int8_t actLineSize;
	for (uint32_t j = *shift; j < *bytesRead; ++j) {
		shiftEndOfLine = isNewLineOrEnd(buff, &j, bytesRead);
		if (shiftEndOfLine != -1) {
			actLineSize = j - *shift - shiftEndOfLine + 1;
			strncpy(line, buff + *shift, actLineSize < lineMaxSize ? actLineSize : lineMaxSize);
			*shift = j + shiftEndOfLine + 1;
			break;
		}
	}
}

uint8_t doRootConfig(const char *buff, const uint32_t *bytesRead, const uint32_t *shift) {
  uint8_t res = parseRootConfig(buff, bytesRead, shift, &newConfStructure);
  if (res == 0) {
    res = setConf(&partitionsStructure, &newConfStructure);
    if (res == 0) {
      res = changePartAndReinitUSB(partitionsStructure.partitions[0].name, partitionsStructure.partitions[0].key);
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
  	res = changePartAndReinitUSB(partName, partKey);
  }
  return res;
}

uint8_t parsePartConfig(const char *buff, const uint32_t *bytesRead, 
												const uint32_t *shift, char *partName, char *partKey) {
  uint8_t res = 1;
  int8_t shiftEndOfLine;
  for (uint32_t i = *shift; i < *bytesRead; ++i) {
    if (buff[i] == ' ') {
      strncpy(partName, buff + *shift, i - *shift);
      for (uint32_t j = i + 1; j < *bytesRead; ++j) {
				shiftEndOfLine = isNewLineOrEnd(buff, &j, bytesRead);
				if (shiftEndOfLine != -1) {
					strncpy(partKey, buff + i + 1, j - i - 1);
					//*shift = j + shiftEndOfLine + 1;
		      res = 0;
					break;
				}
			}
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
            if ((end == buffer) || (*end != '\0')) {
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
                || (strncmp(newPartitionsStructure->partitions[part].key,
                		PUBLIC_PARTITION_KEY,
										PART_KEY_LENGHT) == 0)) {
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
            if ((end == buffer) || (*end != '\0')) {
              break;
            }
            newPartitionsStructure->partitions[part].lastSector = newPartitionsStructure->partitions[part]
								.startSector + newPartitionsStructure->partitions[part].sectorNumber - 1;
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
  if ((res == FR_OK) && (USBD_Stop(&hUsbDeviceFS) == USBD_OK)) {
    formConfFileText(&configFile, partitionsStructure);
    f_close(&configFile);
    HAL_Delay(2000);            // Time delay for host to recognize detachment of the stick
    res = USBD_Start(&hUsbDeviceFS);
  }
  return res;
}

/* Forms file that contains current device configurations */
void formConfFileText(FIL *fil, const PartitionsStructure *partitionsStructure) {
  f_printf(fil, "%s\n", "-------Configurations of The Device---->To save changes please delete this line");
  f_printf(fil, "%s\n", conversion[1].str);      // Update to update configurations
  f_printf(fil, "%s\n", partitionsStructure->rootKey);
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
            SDCardInfo.CardCapacity / SDCardInfo.CardBlockSize - STORAGE_SECTOR_NUMBER);
}

/* Detects and of the line. Works with Windows and Unix line separators */
int8_t isNewLineOrEnd(const char *buff, const uint32_t *position, const uint32_t *size) {
  int8_t shift = -1;
  
  if ((buff[*position] == '\r') || (buff[*position] == '\n') || (*size == *position + 1)) {
    shift = 0;
  }
  if ((buff[*position] == '\r') && (buff[*position + 1] == '\n')) {
    shift = 1;
  }
  return shift;  
}

/* Return true if the command file was updated */
uint8_t isCommandFileUpdated(const FILINFO *fno, const char *fileName, const WORD *lastModifTime) {
  return ((strcmp(fno->fname, fileName) == 0) && (fno->ftime != *lastModifTime)) ? 1 : 0;
}

/* Delete the command file if operation was successful if else renamed it */
void commandExecutionResult(uint8_t res) {
	if (res == 0) {
#if	DEBUG_MOD == 0																	// If the operation was successful command file will be deleted
		f_unlink(COMMAND_FILE_NAME);
#endif
	} else {																					// If the operation was not successful command file will be renamed
		f_rename(COMMAND_FILE_NAME, COMMAND_FILE_NAME_FAILED);
	}
}

