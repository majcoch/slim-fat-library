#include "fileio.h"

#include <stddef.h>
#include <string.h>

#define EOL_CR_CHAR	'\r'
#define EOL_LF_CHAR	'\n'

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

uint8_t find_eol_sequence(const  uint8_t* buff, uint16_t* count) {
	uint8_t match = 0;

	static uint8_t partial_match;
	uint16_t res = 0;
	for (uint16_t i = 0; (i < *count) && !match; i++) {
		if (buff[i] == EOL_CR_CHAR) {
			partial_match = 1;
		}
		else if (buff[i] == EOL_LF_CHAR && partial_match == 1) {
			partial_match = 2;
			match = 1;
		}
		else {
			partial_match = 0;
		}
		res++;
	}
	*count = res;

	return match;
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

fs_error fs_fclose(fs_file_t* file) {
	uint8_t err = FS_SUCCESS;

	if (READ != file->mode) {
		err = fat32_update_entry(file->partition, &file->entry);
	}

	return err;
}

fs_error fs_fflush(fs_file_t* file) {
	uint8_t err = FS_SUCCESS;

	if (READ != file->mode) {
		err = fat32_update_entry(file->partition, &file->entry);
	}

	return err;
}

uint16_t fs_fread(fs_file_t* file, uint8_t* ptr, const uint16_t count) {
	uint8_t err = FS_SUCCESS;

	if (READ != file->mode) {
		err = FS_FILE_ACCES_FAIL;
	}

	uint16_t bytes_left = count;
	uint32_t file_left = get_file_left_bytes(file);
	while (FS_SUCCESS == err && bytes_left && file_left) {
		if (!end_of_cluster(file)) {
			err = fat32_find_next_cluster(file->partition, &file->current_cluster);
		}
		if (FS_SUCCESS == err) {
			err = read_file_buffer(file);
			if (FS_SUCCESS == err) {
				// Calculate bytes to copy from current sector
				uint16_t sector_offset = get_offset_in_sector(file);
				uint16_t bytes_to_copy = SECTOR_SIZE - sector_offset;
				if (bytes_to_copy > file_left) bytes_to_copy = file_left;
				if (bytes_to_copy > bytes_left) bytes_to_copy = bytes_left;

				uint8_t* buffer = get_file_buffer(file);
				memcpy(&ptr[(count - bytes_left)], &buffer[sector_offset], bytes_to_copy);

				bytes_left -= bytes_to_copy;
				file_left -= bytes_to_copy;
				file->current_offset += bytes_to_copy;
			}
		}
	}

	return (count - bytes_left);
}

uint16_t fs_fwrite(fs_file_t* file, const uint8_t* ptr, const uint16_t count){
	uint8_t err = FS_SUCCESS;
	uint16_t bytes_left = count;

	if (READ == file->mode) {
		err = FS_FILE_ACCES_FAIL;
	}

	// Allocate first cluster for empty file
	if (0 == file->entry.file_size) {
		fat32_alloc_new_cluster(file->partition, &file->entry.starting_cluster);
		file->current_cluster = file->entry.starting_cluster;
	}

	while (FS_SUCCESS == err && bytes_left) {
		if (!end_of_cluster(file)) {
			fat32_alloc_new_cluster(file->partition, &file->current_cluster);
		}
		if (FS_SUCCESS == err) {
			err = read_file_buffer(file);
			if (FS_SUCCESS == err) {
				set_pending_write(file->partition->device);

				// Calculate bytes to copy to current sector
				uint16_t sector_offset = get_offset_in_sector(file);
				uint16_t bytes_to_copy = SECTOR_SIZE - sector_offset;
				if (bytes_to_copy > bytes_left) bytes_to_copy = bytes_left;

				uint8_t* buffer = get_file_buffer(file);
				memcpy(&buffer[sector_offset], &ptr[(count - bytes_left)], bytes_to_copy);

				bytes_left -= bytes_to_copy;
				file->entry.file_size += bytes_to_copy;
				file->current_offset += bytes_to_copy;
			}
			
		}
	}

	return (count - bytes_left);
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

uint8_t* fs_fgets(fs_file_t* file, uint8_t* str, const uint16_t num) {
	fs_error err = FS_SUCCESS;
	uint8_t end_of_line = 0;

	uint16_t result_offset = 0;
	uint32_t file_left = get_file_left_bytes(file);
	while (FS_SUCCESS == err && !end_of_line && 0 != file_left) {
		if ( !end_of_cluster(file) ){
			err = fat32_find_next_cluster(file->partition, &file->current_cluster);
		}
		if (FS_SUCCESS == err) {
			err = read_file_buffer(file);
			if (FS_SUCCESS == err) {
				uint16_t sector_offset = get_offset_in_sector(file);
				uint16_t bytes_to_copy = SECTOR_SIZE - sector_offset;
				uint16_t result_left = num - result_offset;
				if (bytes_to_copy > file_left) bytes_to_copy = file_left;
				if (bytes_to_copy > result_left) bytes_to_copy = result_left;

				uint8_t* buffer = get_file_buffer(file);
				end_of_line = find_eol_sequence(&buffer[sector_offset], &bytes_to_copy);
				memcpy(&str[result_offset], &buffer[sector_offset], bytes_to_copy);

				file_left -= bytes_to_copy;
				result_offset += bytes_to_copy;
				file->current_offset += bytes_to_copy;
			}
		}
	}

	return end_of_line ? str : NULL;
}

fs_error fs_fputc(fs_file_t* file, const uint8_t character) {
	uint8_t err = FS_SUCCESS;

	// Allocate first cluster for empty file
	if (0 == file->entry.file_size) {
		fat32_alloc_new_cluster(file->partition, &file->entry.starting_cluster);
		file->current_cluster = file->entry.starting_cluster;
	}

	// Allocate next cluster for more file data
	if (!end_of_cluster(file)) {
		fat32_alloc_new_cluster(file->partition, &file->current_cluster);
	}

	err = read_file_buffer(file);   // Make sure internal buffer is valid
	if (FS_SUCCESS == err) {
		set_pending_write(file->partition->device);

		uint16_t sector_offset = get_offset_in_sector(file);
		uint8_t* buffer = get_file_buffer(file);
		if (0 == sector_offset) {
			memset(buffer, 0, SECTOR_SIZE);
		}
		buffer[sector_offset] = character;

		file->current_offset++;
		file->entry.file_size++;
	}
	

	return err;
}

fs_error fs_fputs(fs_file_t* file, const uint8_t* str) {
	fs_error err = FS_SUCCESS;
	uint16_t str_len = strlen(str);
	uint16_t bytes_left = str_len;

	// Allocate first cluster for empty file
	if (0 == file->entry.file_size) {
		fat32_alloc_new_cluster(file->partition, &file->entry.starting_cluster);
		file->current_cluster = file->entry.starting_cluster;
	}

	while (FS_SUCCESS == err && bytes_left) {
		if (!end_of_cluster(file)) {
			fat32_alloc_new_cluster(file->partition, &file->current_cluster);
		}
		if (FS_SUCCESS == err) {
			err = read_file_buffer(file);
			if (FS_SUCCESS == err) {
				set_pending_write(file->partition->device);

				// Calculate bytes to copy to current sector
				uint16_t sector_offset = get_offset_in_sector(file);
				uint16_t bytes_to_copy = SECTOR_SIZE - sector_offset;
				if (bytes_to_copy > bytes_left) bytes_to_copy = bytes_left;

				uint8_t* buffer = get_file_buffer(file);
				memcpy(&buffer[sector_offset], &str[(str_len - bytes_left)], bytes_to_copy);

				bytes_left -= bytes_to_copy;
				file->entry.file_size += bytes_to_copy;
				file->current_offset += bytes_to_copy;
			}

		}
	}

	return err;
}

fs_error fs_fseek(fs_file_t* file, const uint32_t offset, const fs_seek origin) {
	fs_error err = FS_SUCCESS;

	uint32_t new_offset = 0;
	switch (origin) {
		case FS_SEEK_SET:
			new_offset = offset;
		break;
		
		case FS_SEEK_CUR:
			new_offset = file->current_offset + offset;
		break;
		
		case FS_SEEK_END:
			new_offset = file->entry.file_size - offset;
		break;
	}

	if (new_offset <= file->entry.file_size) {
		// Prevent loading cluster ahead of reading -> load cluster only if read is requested
		uint8_t cluster_number = new_offset / (file->partition->sectors_per_cluster * SECTOR_SIZE);
		if (cluster_number) cluster_number -= !(new_offset % (file->partition->sectors_per_cluster * SECTOR_SIZE));

		uint32_t new_cluster = file->entry.starting_cluster;
		for (uint8_t i = 0; i < cluster_number && !err; i++) {
			err = fat32_find_next_cluster(file->partition, &new_cluster);
		}
		if (FS_SUCCESS == err) {
			file->current_offset = new_offset;
			file->current_cluster = new_cluster;
		}
	}
	else {
		err = FS_INVALID_OFFSET;
	}

	return err;
}

uint32_t fs_ftell(const fs_file_t* file) {
	return file->current_offset;
}

uint8_t fs_feof(const fs_file_t* file) {
	return (0 == get_file_left_bytes(file));
}
