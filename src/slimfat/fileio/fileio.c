#include "fileio.h"

#include <stddef.h>
#include <string.h>

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