#include "fileio.h"

#include <stddef.h>
#include <string.h>

uint8_t end_of_cluster(fs_file_t* file) {
	uint32_t left = file->current_offset % (file->partition->sectors_per_cluster * SECTOR_SIZE);
	return (left || !file->current_offset); // zero on success
}

uint32_t get_file_left_bytes(const fs_file_t* file) {
	return file->entry.file_size - file->current_offset;
}

uint16_t get_offset_in_sector(fs_file_t* file) {
	return file->current_offset % SECTOR_SIZE;
}

fs_error read_file_buffer(fs_file_t* file) {
	uint32_t sector = fat32_get_cluster_sector(file->partition, &file->current_cluster);
	sector += (file->current_offset % (file->partition->sectors_per_cluster * SECTOR_SIZE)) / SECTOR_SIZE;
	return read_buffered_sector(file->partition->device, sector);
}

uint8_t* get_file_buffer(fs_file_t* file) {
	return get_raw_buffer(file->partition->device);
}


fs_error fs_mount(fs_partition_t* partition, const uint8_t partition_number) {
	fs_error err = FS_SUCCESS;
	uint32_t start_sector = 0x00000000;
	err = find_partition(partition->device, partition_number, &start_sector);
	if (FS_SUCCESS == err) {
		err = fat32_mount_partition(partition, start_sector);
	}
	return err;
}

fs_error fs_fopen(fs_file_t* file, const char* file_name, const fs_mode mode) {
	fs_error err = FS_SUCCESS;

	file->mode = mode;
	file->entry.starting_cluster = file->partition->root_cluster;    // search from root directory

	uint8_t offset = 0;
	uint8_t length = 0;
	const char* current = file_name;
	const char* next = file_name;
	do {
		current = next + offset;
		next = strpbrk(current, "/");
		if (NULL != next) {
			// Dir access
			length = next - current;
			offset = 1; // Set offset to skip "/" character
			err = fat32_find_entry(file->partition, &file->entry, current, length);
		}
		else {
			// File access
			length = strlen(current);
			err = fat32_find_entry(file->partition, &file->entry, current, length);
			if (FS_SUCCESS == err) {
				if (READ == mode) {
					file->current_cluster = file->entry.starting_cluster;
					file->current_offset = 0;
				}
				else if (WRITE == mode) {
					fat32_free_cluster_chain(file->partition, &file->entry.starting_cluster);
					file->current_cluster = 0;
					file->current_offset = 0;
					file->entry.file_size = 0;
				}
				else if (APPEND == mode) {
					file->current_cluster = file->entry.starting_cluster;
					fs_fseek(file, file->entry.file_size, FS_SEEK_SET);
				}
			}
			else if (FS_FILE_NOT_FOUND == err && WRITE == mode) {
				err = fat32_create_entry(file->partition, &file->entry, current, length);
			}
		}
	} while (FS_SUCCESS == err && NULL != next);

	return err;
}

uint8_t fs_fgetc(fs_file_t* file) {
	fs_error err = FS_SUCCESS;
	uint8_t result = 0; // This should be EOF character
	
	if (0 != get_file_left_bytes(file)) {
		if ( !end_of_cluster(file) ) {
			err = fat32_find_next_cluster(file->partition, &file->current_cluster);
		}
		if (FS_SUCCESS == err) {
			err = read_file_buffer(file);
			if (FS_SUCCESS == err) {
				uint16_t sector_offset = get_offset_in_sector(file);
				result = file->partition->device->buffer[sector_offset];
				file->current_offset++;
			}
		}
	}

	return result;
}

