# SlimFAT
SlimFAT is a full featured yet lightweight implementation of FAT32 file system dedictaed to be used on embedded system. 

## Table of Contents
1. [About the Project](#about-the-project)
1. [Project Status](#project-status)
1. [Getting Started](#getting-started)
    1. [Getting the Source](#getting-the-source)
    1. [Dependencies](#dependencies)
    1. [Building](#building)
    1. [Usage](#usage)
1. [Versioning](#versioning)

# About the Project
SlimFAT started as an engineering thesis at Silesian University of Technology in Gliwice. It has now come a long way especially in terms of user API and internal architecture. Although there are many implementations of FAT32 file system for microcontrollers this one is unique when it comes to buffering file access. Single buffer per storage device makes it extremly efficient RAM wise when it comes to accessing simultaneously multiple files stored on a single storage device. User API inspired by C standard I/O library makes SlimFAT super intuitive when it comes to getting started. Some of key features:
* Single buffer per storage device
* API inspired by C standard I/O
* Reading existing files
* Writing existing files in append or write (truncation to zero) moder
* Creating new files 
* Access files stored in directories and subdirectories

# Project Status
At this point library is fully functional and provides all basic features that user may expect when it comes to accessing files on embedded system. Optimizations in memmory usage and execution time are still in progress and will be implemented in future releases. Even though test platform for development purposes is ATMega328p SlimFAT will be tested on other platforms in the future. Some minor changes to the user API are still required.

# Getting Started
If you are interested in expanding the capabilities of SlimFAT just download the contents of this repository and launch Microchip Studio solution file located in `slim-fat-sln` folder. This will open the full solution used to develop SlimFAT library.


## Getting the Source
This project is [hosted on GitHub](https://github.com/majcoch/slim-fat-library). You can clone this project directly using this command:
```
git clone https://github.com/majcoch/slim-fat-library
```

## Dependencies
Although SlimFAT is a standalone library it uses some of C standard libraries and int types. Most of modern C compilers come with everything that is needed to successfuly build and use this library. It has to be noted that FAT32 file system uses little-endian format and so it is expected that the target platform uses the same one - if not `memcpy` function may cause problems and invalid calculations may be carried out when accessing file system.

## Building
In order to successfully build this library you need to have C99 compiler or later. At this point there is no makefile provided.

## Usage
In order to use SlimFAT in your project you need to include `slimfat/slimfat.h` in your source. This will give you access to library API functions and types used to access files stored on your selected storage device.

### Storage media initialization
SlimFAT comes pre-packaged with SD card driver. If you want to use it you need to include `sd-driver/sd-driver.h` in your source. In order to initialize SD card and access raw bytes you need to register two functions to communicate with SD card via SPI bus - transfering single byte and selecting active device via chip select line. As this is platform specific no implementation of SPI communication is provided. Example below shows how should said functions be implemented and registered.
```c
uint8_t spi_master_transfer(uint8_t byte) {
	SPDR = byte;
	while(!(SPSR & (1<<SPIF)));
	return SPDR;
}

void spi_slave_sd_select(uint8_t state) {
	if(state) PORTB |= (1<<PORTB1);
	else PORTB &= ~(1<<PORTB1);
}

// Create handle for SD card connected to SPI
sd_card_t sd_card = GET_SD_HANDLE(spi_master_transfer, spi_slave_sd_select);
```
As you can see SD card is a single `sd_card_t` object which means you can connect and access as many SD cards as needed. From now on you can easily initialize SD card. Example shows how to initialize SD card object
```c
sd_card_err err = sd_card_init(&sd_card);
if(SD_SUCCESS == err) {
  // SD card read and write can be performed
 }
```

### File system initialization
File system portion of SlimFat library consist of three layers:
* Storage layer - responsible for buffering access to the storage device and providing abstraction layer for storage media access driver
* FAT32 file system layer - responsible for interpreting data stored in file system structures
* fileio layer - user API and file access (read/write) handling
In order to iniliazie file system first a storage abstraction object needs to be created and initialized. If you want to use provided SD card driver you can simply follow the example:
```c
// Device buffer
uint8_t buffer[SECTOR_SIZE] = { 0 };

// Create handle for generic storage device with sector buffer
fs_storage_device storage_dev = GET_DEV_HANDLE(buffer, &sd_card, sd_card_read, sd_card_write);
```
In first step a device buffer is created. SD card driver provides macro `SECTOR_SIZE` which evaluates to `512` as this is default hardware sector size. Next `fs_storage_device` is created and initialized with the buffer, SD card object and two functions - `sd_card_read` and `sd_card_write` for reading and writing single sector. This step provides abstraction and enables you to use any type of storage media access driver you have implemented but preserve unique capability of device buffering.

Now that you have created `fs_storage_device` object you can proceed to creating and mounting a partition. Partition is exactly what it says - a partition on storaged device which has been formated with FAT32 file system. Example shows how to create and intiialize a partition.
```c
// Create handle for partition to be mounted
fs_partition_t partition = GET_PART_HANDLE(storage_dev);
if (FS_SUCCESS == fs_mount(&partition, 0)) {
  // File access can now be performed
}
```
After successfull initialization of selected partition you can now access files.

### File Reading
This example shows how to access exisitng file for reading.
```c
/* Read existing files on mounted partition */
fs_file_t read_file = GET_FILE_HANDLE(partition);
if (FS_SUCCESS == fs_fopen(&read_file, "read.txt", READ)) {
  uint8_t line_buff[100] = { 0 };
  /* Access data stored in file */
  fs_fgetc(&read_file);
  fs_fgets(&read_file, line_buff, sizeof(line_buff));
	  fs_fread(&read_file, line_buff, 100);

  /* Navigate file */
  fs_fseek(&read_file, 300, FS_SEEK_SET);
  
  fs_fclose(&read_file);
}
```
## File Writing
This example shows how to access file for writing. If file does not exist it will be created. If it existis its size will be truncted to zero and contents wiped.
```c
fs_file_t write_file = GET_FILE_HANDLE(partition);
if (FS_SUCCESS == fs_fopen(&write_file, "write.txt", WRITE)) {
  uint8_t line_buff[100] = { 0 };
  /* Store data to file */
  fs_fputc(&write_file, 'a');
  fs_fputs(&write_file, "Hello, world!");
  fs_fflush(&write_file);
  fs_fwrite(&write_file, line_buff, 100);

  /* Close file to make sure data is flushed */
  fs_fclose(&write_file);
}
```

# Versioning
This project uses [Semantic Versioning](http://semver.org/). For a list of available versions, see the [repository tag list](https://github.com/majcoch/slim-fat-library/tags).
