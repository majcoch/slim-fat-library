/*
 * slim-fat-runner.c
 *
 * Created: 03.09.2020 15:49:29
 * Author : Micha³ Granda
 */ 

#include <avr/io.h>

// Device SPI driver
#include "spi/spi_master.h"

// Library supplied SD card driver
#include "sd-driver/sd_driver.h"

// SlimFAT file system driver
#include "slimfat/slimfat.h"

// Device buffer
uint8_t buffer[SECTOR_SIZE] = { 0 };

int main(void) {
	spi_master_init();
	
	// Create handle for SD card connected to SPI
	sd_card_t sd_card = GET_SD_HANDLE(spi_master_transfer, spi_slave_sd_select);
	
	// Create handle for generic storage device with sector buffer
	fs_storage_device storage_dev = GET_DEV_HANDLE(buffer, &sd_card, sd_card_read, sd_card_write);
	
	// Create handle for partition to be mounted
	fs_partition_t partition = GET_PART_HANDLE(storage_dev);
	
	// Initialize SD card
	sd_card_err err = sd_card_init(&sd_card);
	if(SD_SUCCESS == err) {
		// Mount selected partition (0 - 3)
		if (FS_SUCCESS == fs_mount(&partition, 0)) {
			uint8_t line_buff[100] = { 0 };
							
			/* Read existing files on mounted partition */
			fs_file_t read_file = GET_FILE_HANDLE(partition);
			if (FS_SUCCESS == fs_fopen(&read_file, "read.txt", READ)) {
				/* Access data stored in file */
				fs_fgetc(&read_file);
				fs_fgets(&read_file, line_buff, sizeof(line_buff));
				fs_fread(&read_file, line_buff, 100);

				/* Navigate file */
				fs_fseek(&read_file, 300, FS_SEEK_SET);
			}
			
			/* Write existing files or create new on mounted partition */
			fs_file_t write_file = GET_FILE_HANDLE(partition);
			if (FS_SUCCESS == fs_fopen(&write_file, "write.txt", WRITE)) {
				/* Store data to file */
				fs_fputc(&write_file, 'a');
				fs_fputs(&write_file, "Hello, world!");
				fs_fflush(&write_file);
				fs_fwrite(&write_file, line_buff, 100);
				
				/* Close file to make sure data is flushed */
				fs_fclose(&write_file);
			}
			
			/* Append existing file on mounted partition */
			fs_file_t append_file = GET_FILE_HANDLE(partition);
			if (FS_SUCCESS == fs_fopen(&append_file, "append.txt", APPEND)) {
				/* Store data to file */
				fs_fputs(&append_file, "This is simple append.");
				fs_fclose(&append_file);
			}
			
		}
	}
    while (1);
}

