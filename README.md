# Double Bottom USB Stick
USB Stick that can conceal and encrypt its partitions. The project is completely student work. 

The project meets all goals expectations and fully operational. However, during the development was proposed the improvements that were not implemented. We recommend to visit category [Future Improvements](https://github.com/Lrakulka/Double_Bottom_USB_Stick#future-improvements).

[Google Summer of Code 2017](https://summerofcode.withgoogle.com/projects/#5177270082732032)

Student: Oleksandr Borysov

Project Name: USB Flash Drive with possibility to conceal information (Double Bottom USB Stick)

Organization: Portland State University

Mentor: Keith Packard

The last commit during GSoC program .... Tag   ....
# Project Brief
The main goal of the project is the creation of a USB Flash Drive with the possibility to conceal the fact that the flash drive contains hidden information. It means that hidden folders and files will not be displayed if the USB drive is connected to a computer. The microcontroller of the Double Bottom USB Stick will carry the process of encryption and concealment of files and folders. 

The only possibility to break protection will be physically reading of memory from the memory component and breaking the encryption algorithm.

# Project Technologies And Hardware
* Test board is NUCLEO [STM32F446RE](https://developer.mbed.org/platforms/ST-Nucleo-F446RE/);
* SDIO Micro SD Card reader module;
* FS USB 2.0 cable;
* Micro SD 4 GB class 4, HC;
* A large part of the project generated with help of STM32CubeMX v4.18 Firmware v1.14 ([Cube project file](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/Double_Bottom_USB_Stick.ioc));
* TRUEStudio was used for the project development;
* Dependiaces: SDIO intarface, FS USB 2.0 Mass Storage Device intaface, [FatFS](http://elm-chan.org/fsw/ff/00index_e.html)
# Board Schematic
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Double_Bottom_USB_Stick_Sketch_bb-min.png)
# My Test Board
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Project_Assembled.jpg)
# Getting Started
1) Download and install [ST-Link](http://www.st.com/content/st_com/en/products/embedded-software/development-tool-software/stsw-link009.html);
2) Download and install [TRUEStudio](https://atollic.com/truestudio/);
3) Clone the project or download zip file;
4) Open TRUEStudio. Select File -> Import... -> General -> Existing Project into Workspace -> Press "Next >" button -> Browse to the project folder -> Select "Double_Bottom_USB_Stick" in the Project window -> Press "Finish";
5) Connect to the PC the board;
6) Select in the studio one of the project's files and press "Debug" (Green bug)
7) Optional. Install [STM32CubeMX](http://www.st.com/en/development-tools/stm32cubemx.html) and open Cube project [Double_Bottom_USB_Stick.ioc](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/Double_Bottom_USB_Stick.ioc) with the project configurations.
# How It Works
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Current_Device_Memory_Allocation.PNG)
# User Commands
At this moment the device supports four commands:
* Initializes device default configurations (```InitConf - INIT_DEVICE_CONFIGURATIONS```)
* Changes currently visible partition (```ChangePart - CHANGE_PARTITION```)
* Shows device configurations (```ShowConf - SHOW_ROOT_CONFIGURATIONS```)
* Updates device configurations (```UpdateConf - UPDATE_ROOT_CONFIGURATIONS```)

### InitConf ###
Create in the root directory of the device a file with name ```COMMAND_.TXT - COMMAND_FILE_NAME``` and fill it with the following text.
```
InitConf
[Device Unique ID - DEVICE_UNIQUE_ID]
```
If device ID is correct then the command file will be deleted and the device will reconnect connection with the host, if ID or command not correct no action will be executed.

### ChangePart ###
Create in the root directory of the device a file with name ```COMMAND_.TXT - COMMAND_FILE_NAME``` and fill it with the following text.
```
ChangePart
[Device root key]
[Partition name] [Partition key]
```
If root key, partition key and partition name are correct then the command file will be deleted and the device will reconnect connection with the host and switch the currently visible partition, if root key not correct no action will be executed, if the partition name or key is incorrect then the command file will be rename to ```COMMAND_FILE_NAME_FAILED - COMMANDF.TXT``` (If the partition is public then You should write "public" as partition key.

### ShowConf ###
Create in the root directory of the device a file with name ```COMMAND_.TXT - COMMAND_FILE_NAME``` and fill it with the following text.
```
ShowConf
[Device root key]
[Configurations key]
```
If device root key and configuration key are correct then the command file will be deleted and the device will reconnect connection with the host and will create a file ```DEVICE_CONFIGS - CONFIGS_.TXT``` with the device configurations, if the root key or configuration key is not correct then no action will be executed.

### UpdateConf ###
Create in the root directory of the device a file with name ```COMMAND_.TXT - COMMAND_FILE_NAME``` and fill it with the following text.
```
UpdateConf
[Device root key]
[Configurations key]
[New configurations key] <--- Key for revealing device configurations
[New device root key] <--- Root Key of the device
#N___________Name___________Key___________Number of sectors
0  part0                public               3872257   
1  part1                part1Key             3870000  
2  part2                part2Key             2000  
3  part3                part3Key             253  
[New partition number] [New partition name] [New partition key] [New partition memory size]
-------------SD card available memory-------------
3965190144          <- Card capacity memory	
512                 <- Card block size	
7744510             <- Card block sector number	
```
Note: if write "public" as [New partition key] the partition will be Pablic. Also, you can delete partitions as well (Just delete it from the update configuration file).

If device's root key and configuration key are correct the command file will be deleted and the device will reconnect connection with the host and the device configurations will be updated, also the currently visible partition will be switched to partition 0, if the root key or configuration key is not correct then no action will be executed, if update operation feils with error the command file will be renamed to ```COMMAND_FILE_NAME_FAILED - COMMANDF.TXT```.

Note: Any commands that failed with error during execution will force the device to rename the command file to  ```COMMAND_FILE_NAME_FAILED - COMMANDF.TXT```.
# Future Improvements
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Future_Device_Memory_Allocation.png)
# Detail Images of the Test Board
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Board.jpg)
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Card_Reader-And_MicroSD_Card.jpg)
# More
The project [issues](https://github.com/Lrakulka/Double_Bottom_USB_Stick/issues) contains useful information related to the project

