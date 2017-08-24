# Double Bottom USB Stick
USB Stick that can conceal and encrypt its partitions. **The project is 100% student work.** 

The project meets all goals and fully operational. However, during the development was proposed improvements that were not implemented. We recommend to visit category [Future Improvements](https://github.com/Lrakulka/Double_Bottom_USB_Stick#future-improvements).

[Google Summer of Code 2017](https://summerofcode.withgoogle.com/projects/#5177270082732032)

Student: Oleksandr Borysov

Project Name: USB Flash Drive with possibility to conceal information (Double Bottom USB Stick)

Organization: Portland State University

Mentor: Keith Packard

Most of the work was done in the files sd_io_controller.\* and user_interface.\*
# Project Brief
The main goal of the project is the creation of USB Flash Drive with the possibility to conceal the fact that the flash drive contains hidden information. It means that hidden folders and files will not be displayed if the USB drive is connected to computer. The microcontroller of the Double Bottom USB Stick will carry the process of encryption and concealment of files and folders. 

The only possibility to break protection is physically read a memory from the memory component and breaking the encryption algorithm.

# Demonstration Video
[![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Double_Bottom_USB_Stick_Video.png)](https://youtu.be/ZjI67Ov26jA);

# Installation Instruction
1) Download and install [ST-Link](http://www.st.com/content/st_com/en/products/embedded-software/development-tool-software/stsw-link009.html);
2) Download and install [TRUEStudio](https://atollic.com/truestudio/);
3) Download and install [STM32CubeMX v4.18](https://community.st.com/thread/39835-please-provide-a-way-to-download-older-version-of-the-cube);
3) Clone the project or download zip file;
4) Open CubeMX project file ```Double_Bottom_USB_Stick.ioc```;
5) Generate STM source code (Button with Gear);
6) Open the project in the command prompt;
7) Execute command: ```git apply --reject --whitespace=fix Patch-for-STM-files.patch```;
8) Delete the directory ```Double_Bottom_USB_Stick\Middlewares\Third_Party\FatFs\src\drivers``` in the project;
9) Open TRUEStudio. Select File -> Import... -> General -> Existing Project into Workspace -> Press "Next >" button -> Browse to the project folder -> Select "Double_Bottom_USB_Stick" in the Project window -> Press "Finish";
10) Connect  the board to the PC;
11) Select in the project explorer one of the project's files and press "Debug" (Button with Green bug)

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

# How It Works
The logic behind the device is to represent a part of SD Card memory as the solid memory of the device (Double Bottom USB Stick).
The project present itself as regular USB Mass Storage Device (USB 2.0 Stick) and currently visible partition memory represents as USB Stick memory to the host. Zero partition is always public because this partition displays when the device physically connected to the host. Also, this partition will be demonstrated to the user if connect SD Card without the device to the host. The partitions divided into two categories: public and private. The private partitions are encrypted/decrypted by XOR cipher on the fly using the partition key. 
The device configurations encryption/decryption by AES cipher at the saving/loading configurations to/from the SD Card using the root key.
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Current_Device_Memory_Allocation.PNG)
# User Commands
Currently, the device supports four commands:
* Initializes device default configurations (```InitConf - INIT_DEVICE_CONFIGURATIONS```)
* Changes currently visible partition (```ChangePart - CHANGE_PARTITION```)
* Shows device configurations (```ShowConf - SHOW_ROOT_CONFIGURATIONS```)
* Updates device configurations (```UpdateConf - UPDATE_ROOT_CONFIGURATIONS```)

### InitConf ###
Create in the root directory of the device a file with name ```COMMAND_.TXT - COMMAND_FILE_NAME``` and fill it with the following text.
```
InitConf
[Device Unique ID - DEVICE_UNIQUE_ID]
// Empty line
```
If device ID is correct then the command file will be deleted and the device will reconnect to the host, if ID or command not correct no action will be executed.

### ChangePart ###
Create in the root directory of the device a file with name ```COMMAND_.TXT - COMMAND_FILE_NAME``` and fill it with the following text.
```
ChangePart
[Device root key]
[Partition name] [Partition key]
// Empty line
```
If root key, partition key and partition name are correct then the command file will be deleted and the device will reconnect to the host and switch the currently visible partition. If root key not correct no action will be executed. If the partition name or key is incorrect then the command file will be renamed to ```COMMAND_FILE_NAME_FAILED - COMMANDF.TXT``` (If the partition is public then You should write "public" as partition key).

### ShowConf ###
Create in the root directory of the device a file with name ```COMMAND_.TXT - COMMAND_FILE_NAME``` and fill it with the following text.
```
ShowConf
[Device root key]
[Configurations key]
// Empty line
```
If device root key and configuration key are correct then the command file will be deleted and the device will reconnect to the host and will create a file ```DEVICE_CONFIGS - CONFIGS_.TXT``` with the device configurations. If the root key or configuration key is not correct then no action will be executed.

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
// Empty line
```
Note: if write "public" as [New partition key] the partition will be public. Also you can delete partitions as well (Just delete it from the update configuration file).

If device root key and configuration key are correct the command file will be deleted and the device will reconnect to the host and the device configurations will be updated, also the currently visible partition will be switched to partition 0. If the root key or configuration key is not correct then no action will be executed. If update operation fails with error the command file will be renamed to ```COMMAND_FILE_NAME_FAILED - COMMANDF.TXT```.

Note 1: Any commands that failed with error during execution will rename the command file to  ```COMMAND_FILE_NAME_FAILED - COMMANDF.TXT```.
Note 2: The project has constants that created for debug mode ```DEBUG_MOD``` and ```CIPHER_MOD```(Constans changes behavior of the device)
# Future Improvements
At this moment each partition part of SD Card memory is allocated as a solid piece. This approach would be acceptable if the project used for the partition encryption AES but due to its performance cost the device uses XOR cipher. That is why the spreading memory of the each partition across the SD Card memory will increase the level of data protection. Also, the logic for the forming XOR long key must be more complex to increase XOR cipher reliability.
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Future_Device_Memory_Allocation.png)
# My Test Board
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Project_Assembled.jpg)
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Board.jpg)
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Card_Reader-And_MicroSD_Card.jpg)
# More
The project [issues](https://github.com/Lrakulka/Double_Bottom_USB_Stick/issues) contains useful information related to the project.
