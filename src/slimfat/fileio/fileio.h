/*
 * fileio.h
 *
 * Created: 19.11.2020 22:45:34
 * Author : Micha³ Granda
 */


#ifndef STANDARDIO_H
#define STANDARDIO_H

#include <stdint.h>
#include "../slimfaterr.h"
#include "../storage/storage.h"
#include "../fat32/fat32.h"

typedef enum {
	READ,		/* Opens file for read only. File must exist*/

	WRITE,		/* Opens file for write only. 
				 * If file exists its size is truncated to 0.
				 * If it does not exist it is created */

	APPEND		/* Opens file for write only. 
				 * File must exist. Write is performed only
				 * at the end of the file*/
} fs_mode;

typedef enum {
	FS_SEEK_SET,
	FS_SEEK_CUR,
	FS_SEEK_END
} fs_seek;

typedef struct fs_generic_file {
	// Partition on which file exists
	fs_partition_t* partition;
	// FAT version dependent 
	fat_entry_t	entry;
	// Text/binary file access data
	fs_mode		mode;
	uint32_t	current_cluster;
	uint32_t	current_offset;
} fs_file_t;

/* Partition operations */
fs_error fs_mount(fs_partition_t* partition, const uint8_t partition_number);

/* File access */
fs_error fs_fopen(fs_file_t* file, const char* file_name, const fs_mode mode);
fs_error fs_fclose(fs_file_t* file);
fs_error fs_fflush(fs_file_t* file);

/* Direct input/output */
uint16_t fs_fread(fs_file_t* file, uint8_t* ptr, const uint16_t count);
uint16_t fs_fwrite(fs_file_t* file, const uint8_t* buffer, const uint16_t count);

/* Character input/output */
uint8_t  fs_fgetc(fs_file_t* file);
uint8_t* fs_fgets(fs_file_t* file, uint8_t* str, const uint16_t num);
fs_error fs_fputc(fs_file_t* file, const uint8_t character);
fs_error fs_fputs(fs_file_t* file, const uint8_t* str);

/* File positioning */
fs_error fs_fseek(fs_file_t* file, const uint32_t offset, const fs_seek origin);
uint32_t fs_ftell(const fs_file_t* file);

/* Error-handling */
uint8_t fs_feof(const fs_file_t* file);

#endif