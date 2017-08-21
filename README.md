# Double Bottom USB Stick
USB Stick that can conceal and encrypt its partitions. The project is completely student work. 
The project meets all goals expectations and fully operational. However, during the development was proposed the improvements that were not implemented. We recommend to visit category [Future Improvements](https://github.com/Lrakulka/Double_Bottom_USB_Stick#future-improvements).

[Google Summer of Code 2017](https://summerofcode.withgoogle.com/projects/#5177270082732032)
Student: Oleksandr Borysov
Project Name: USB Flash Drive with possibility to conceal information (Double Bottom USB Stick)
Organization: Portland State University
Mentor: Keith Packard

# Project Brief
The main goal of the project is the creation of a USB Flash Drive with the possibility to conceal the fact that the flash drive contains hidden information. It means that hidden folders and files will not be displayed if the USB drive is connected to a computer. The microcontroller of the Double Bottom USB Stick will carry the process of encryption and concealment of files and folders. 

The only possibility to break protection will be physically reading of memory from the memory component and breaking the encryption algorithm.

# Project Technologies And Hardware
* Test board is NUCLEO [STM32F446RE](https://developer.mbed.org/platforms/ST-Nucleo-F446RE/);
* SDIO Micro SD Card reader module;
* FS USB 2.0 cable;
* Micro SD 4 GB class 4, HC;
* A large part of the project generated with help of STM32CubeMX ([Cube project file is Double_Bottom_USB_Stick.ioc](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/Double_Bottom_USB_Stick.ioc));
* For the project development I used TRUEStudio;
* Dependiaces: SDIO intarface, FS USB 2.0 Mass Storage Device intaface, [FatFS](http://elm-chan.org/fsw/ff/00index_e.html)
# Board Schematic
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Double_Bottom_USB_Stick_Sketch_bb-min.png)
# My Test Board
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Project_Assembled.jpg)
# Getting Started
1) Download and install [ST-Link](http://www.st.com/content/st_com/en/products/embedded-software/development-tool-software/stsw-link009.html);
2) Download and install [µVision](http://www2.keil.com/mdk5/install/);
3) Open project [Double_Bottom_USB_Stick.uvprojx](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/MDK-ARM/Double_Bottom_USB_Stick.uvprojx) with µVision;
4) Proceed next steps to compile and load the project sketch on the board;
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Keil_uVision_Build_Load_Project.PNG)
5) Press reset button on the board (Black right button) and observe blinking of the green led and appearance of the new mass storage device on the computer;
6) Optional. Install [STM32CubeMX](http://www.st.com/en/development-tools/stm32cubemx.html) and open Cube project [Double_Bottom_USB_Stick.ioc](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/Double_Bottom_USB_Stick.ioc) with project configurations.
# Detail Images of the Project hardware
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Board.jpg)
![](https://github.com/Lrakulka/Double_Bottom_USB_Stick/blob/master/info/Card_Reader-And_MicroSD_Card.jpg)
# Future Improvements
# More
The project [issues](https://github.com/Lrakulka/Double_Bottom_USB_Stick/issues) contains useful information related to the project

