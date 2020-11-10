#include "fat32.h"

#include <string.h>
#include <ctype.h>

#define ATTR_READ_ONLY	0x01
#define ATTR_HIDDEN		0x02
#define ATTR_SYSTEM		0x04
#define ATTR_VOLUME_ID	0x08
#define ATTR_DIRECTORY	0x10
#define ATTR_ARCHIVE	0x20

#define ENTRY_SIZE  32

#define SECTOR_SIZE 512

#define FS_TYPE_SIG "FAT32"

#define FAT_32_EMPTY_CLUSTER(cluster)
#define FAT_32_EMPTY_ENTRY(entry) 0x00 == entry_buf[0] || 0xE5 == entry_buf[0]

#define CONVERT_TO_FAT_TIME(H, M, S) ((H & 0b0011111) << 11) | ((M & 0b111111) << 5) | (S & 0b11111)
#define CONVERT_TO_FAT_DATE(Y, M, D) ((Y & 0b1111111) << 9 ) | ((M & 0b001111) << 5) | (D & 0b11111)

uint8_t fat32_validate_partition(const uint8_t* buffer) {
	const uint8_t* fat_name = &buffer[0x52];
	return strncmp(fat_name, FS_TYPE_SIG, sizeof(FS_TYPE_SIG) - 1);
}

void fat32_read_volume_boot_record(fs_partition_t* partition, const uint32_t start_sector, const uint8_t* vbr_buf) {
	uint8_t BPB_NumFATs = 0;
	uint16_t BPB_RsvdSecCnt = 0;

	memcpy(&partition->bytes_per_sector, &vbr_buf[0x000B], sizeof(uint16_t));
	memcpy(&partition->sectors_per_cluster, &vbr_buf[0x000D], sizeof(uint8_t));
	memcpy(&BPB_RsvdSecCnt, &vbr_buf[0x000E], sizeof(uint16_t));
	memcpy(&BPB_NumFATs, &vbr_buf[0x0010], sizeof(uint8_t));
	memcpy(&partition->sectors_pre_fat, &vbr_buf[0x0024], sizeof(uint32_t));
	memcpy(&partition->root_cluster, &vbr_buf[0x002C], sizeof(uint32_t));

	partition->fat_start_sector = start_sector + BPB_RsvdSecCnt;
	partition->data_start_sector = start_sector + BPB_RsvdSecCnt + (BPB_NumFATs * partition->sectors_pre_fat);
}

void fat32_read_file_entry(fat_entry_t* file, const uint8_t* entry_buf) {
	memcpy(&file->attributes, &entry_buf[0x0b], sizeof(uint8_t));
	memcpy(&file->file_size, &entry_buf[0x1c], sizeof(uint32_t));
	memcpy(&((uint8_t*)&file->starting_cluster)[1], &entry_buf[0x14], sizeof(uint16_t));
	memcpy(&((uint8_t*)&file->starting_cluster)[0], &entry_buf[0x1a], sizeof(uint16_t));
}

void fat32_write_file_entry(uint8_t* entry_buf, const fat_entry_t* file) {

	uint8_t attribute = 0x20;
	memcpy(&entry_buf[11], &attribute, sizeof(uint8_t));

	uint8_t reserved = 0x18;    // bit 3 - lowercase name
	// bit 4 - lowercase ext
	memcpy(&entry_buf[12], &reserved, sizeof(uint8_t));

	uint8_t creation_time_milis = 0x00;
	memcpy(&entry_buf[13], &creation_time_milis, sizeof(uint8_t));

	uint16_t creation_time = CONVERT_TO_FAT_TIME(12, 10, 10);
	memcpy(&entry_buf[14], &creation_time, sizeof(uint16_t));

	uint16_t creation_date = CONVERT_TO_FAT_DATE(41, 4, 15);
	memcpy(&entry_buf[16], &creation_date, sizeof(uint16_t));

	uint16_t last_access_date = CONVERT_TO_FAT_DATE(41, 4, 15);
	memcpy(&entry_buf[18], &last_access_date, sizeof(uint16_t));

	memcpy(&entry_buf[0x14], &((uint8_t*)&file->starting_cluster)[1], sizeof(uint16_t));

	uint16_t last_write_time = CONVERT_TO_FAT_TIME(12, 10, 10);
	memcpy(&entry_buf[22], &last_write_time, sizeof(uint16_t));

	uint16_t last_write_date = CONVERT_TO_FAT_DATE(41, 4, 15);
	memcpy(&entry_buf[24], &last_write_date, sizeof(uint16_t));

	memcpy(&entry_buf[0x1a], &((uint8_t*)&file->starting_cluster)[0], sizeof(uint16_t));

	memcpy(&entry_buf[0x1c], &file->file_size, sizeof(uint32_t));
}

uint8_t fat32_match_short_name(const uint8_t* entry_buf, const uint8_t* name, const uint8_t length) {
	size_t j = 0;
	for (size_t i = 0; i < 11 && j < length; i++) {
		if (entry_buf[i] != ' ') {
			if ('.' == name[j]) j++;	// skip dot between name and ext
			if (tolower(entry_buf[i]) != name[j]) return 1;
			j++;
		}
	}
	return 0;
}

void fat32_write_short_name(uint8_t* entry_buf, const uint8_t* name, const uint8_t length) {
	size_t j = 0;
	uint8_t padding = 0;
	size_t i = 0;
	for (; i < 8; i++) {
		if (name[i] == '.') {
			padding = 1;
			j++;
		}
		if (padding) {
			entry_buf[i] = ' ';
		}
		else {
			entry_buf[i] = toupper(name[j]);
			j++;
		}
	}

	for (; i < 11; i++) {
		entry_buf[i] = toupper(name[j]);
		j++;
	}
}

fs_error fat32_clear_cluster(const fs_partition_t* partition, const uint32_t* cluster) {
	fs_error err = FS_SUCCESS;

	uint32_t sector_to_clean = fat32_get_cluster_sector(partition, cluster);
	uint8_t sectors_per_cluster = partition->sectors_per_cluster;
	sector_to_clean += (sectors_per_cluster - 1);   // Start clearing cluster's sectors from the end
	for (uint8_t sector = 0; sector < sectors_per_cluster; sector++) {
		memset(get_raw_buffer(partition->device), 0, SECTOR_SIZE);
		err = write_buffered_sector(partition->device, sector_to_clean - sector);
	}

	return err;
}


fs_error fat32_mount_partition(fs_partition_t* partition, const uint32_t start_sector) {
	fs_error err = FS_SUCCESS;

	err = read_buffered_sector(partition->device, start_sector);    // Expected to see VBR (Volume Boot Record)
	if (FS_SUCCESS == err) {
		uint8_t* boot_sector = get_raw_buffer(partition->device);
		if (!fat32_validate_partition(boot_sector)) {
			fat32_read_volume_boot_record(partition, start_sector, boot_sector);
		}
		else {
			err = FS_UNSUPPORTED_FS;
		}
	}

	return err;
}

fs_error fat32_find_entry(const fs_partition_t* partition, fat_entry_t* entry, const uint8_t* name, const uint8_t name_len) {
	fs_error err = FS_SUCCESS;
	
	uint32_t dir_cluster = entry->starting_cluster;
	while (FS_SUCCESS == err) {
		uint32_t sector = fat32_get_cluster_sector(partition, &dir_cluster);
		for (uint8_t sector_id = 0; sector_id < partition->sectors_per_cluster; sector_id++) {
			err = read_buffered_sector(partition->device, (sector + sector_id));
			if (FS_SUCCESS == err) {
				uint8_t* dir_buff = get_raw_buffer(partition->device);
				for (uint16_t entry_offset = 0; entry_offset < SECTOR_SIZE; entry_offset += ENTRY_SIZE) {
					uint8_t* entry_buf = &dir_buff[entry_offset];
					if (0x00 == entry_buf[0]) {
						return FS_FILE_NOT_FOUND;
					}
					else if (!fat32_match_short_name(entry_buf, name, name_len)) {
						fat32_read_file_entry(entry, entry_buf);
						entry->root_dir_cluster = dir_cluster;
						entry->root_dir_offset = entry_offset;
						return FS_SUCCESS;
					}
				}
			}
		}
		err = fat32_find_next_cluster(partition, &dir_cluster);
		if (FS_END_OF_CHAIN == err) {
			err = FS_FILE_NOT_FOUND;
		}
	}

	return err;
}

fs_error fat32_create_entry(const fs_partition_t* partition, fat_entry_t* entry, const uint8_t* name, const uint8_t name_len) {
	fs_error err = FS_SUCCESS;

	uint32_t dir_cluster = entry->starting_cluster;
	while (FS_SUCCESS == err) {
		uint32_t sector = fat32_get_cluster_sector(partition, &dir_cluster);
		for (uint8_t sector_id = 0; sector_id < partition->sectors_per_cluster; sector_id++) {
			err = read_buffered_sector(partition->device, (sector + sector_id));
			if (FS_SUCCESS == err) {
				uint8_t* dir_buff = get_raw_buffer(partition->device);
				for (uint16_t entry_offset = 0; entry_offset < SECTOR_SIZE; entry_offset += ENTRY_SIZE) {
					uint8_t* entry_buf = &dir_buff[entry_offset];
					if (FAT_32_EMPTY_ENTRY(entry_buf)) {
						entry->starting_cluster = 0;
						entry->file_size = 0;
						entry->root_dir_cluster = dir_cluster;
						entry->root_dir_offset = entry_offset;
						set_pending_write(partition->device);
						fat32_write_short_name(entry_buf, name, name_len);
						fat32_write_file_entry(entry_buf, entry);
						return FS_SUCCESS;
					}
				}
			}
		}
		err = fat32_find_next_cluster(partition, &dir_cluster);
		if (FS_END_OF_CHAIN == err) {
			err = fat32_alloc_new_cluster(partition, &dir_cluster);
		}
	}
	return err;
}

fs_error fat32_update_entry(const fs_partition_t* partition, fat_entry_t* file) {
	fs_error err = FS_SUCCESS;

	uint32_t sector = fat32_get_cluster_sector(partition, &file->root_dir_cluster);
	sector += file->root_dir_offset / SECTOR_SIZE;
	err = read_buffered_sector(partition->device, sector);
	if (FS_SUCCESS == err) {
		uint8_t* buffer_entry = &get_raw_buffer(partition->device)[file->root_dir_offset];
		fat32_write_file_entry(buffer_entry, file);
		err = write_buffered_sector(partition->device, sector);
	}

	return err;
}

uint32_t fat32_get_cluster_sector(const fs_partition_t* partition, const uint32_t* cluster) {
	return (partition->data_start_sector + (*cluster - 2) * partition->sectors_per_cluster);
}

fs_error fat32_find_next_cluster(const fs_partition_t* partition, uint32_t* cluster) {
	fs_error err = FS_SUCCESS;

	uint32_t next_FAT_entry;
	uint32_t current_FAT_entry = *cluster * 4;
	uint32_t current_FAT_sector = partition->fat_start_sector + (current_FAT_entry / SECTOR_SIZE);  // counted from zero
	
	err = read_buffered_sector(partition->device, current_FAT_sector);
	if (err == FS_SUCCESS) {
		uint8_t* fat_entry_buff = &partition->device->buffer[current_FAT_entry % SECTOR_SIZE];
		memcpy(&next_FAT_entry, fat_entry_buff, sizeof(uint32_t));
		// TODO change to validation for correct file entry
		if (0x0fffffff != next_FAT_entry) {
			*cluster = next_FAT_entry;
		}
		else {
			err = FS_END_OF_CHAIN;
		}
	}

	return err;
}

fs_error fat32_alloc_new_cluster(const fs_partition_t* partition, uint32_t* last_cluster) {
	fs_error err = FS_SUCCESS;

	uint8_t found = 0;
	uint32_t free_cluster = 0;
	for (uint32_t sector = 0; sector < partition->sectors_pre_fat && !found; sector++) {
		err = read_buffered_sector(partition->device, partition->fat_start_sector + sector);
		for (uint8_t entry = 0; entry < 128 && FS_SUCCESS == err && !found; entry++) {
			memcpy(&free_cluster, &partition->device->buffer[entry * 4], sizeof(uint32_t));
			if (0 == free_cluster) {
				found = 1;

				free_cluster = 0x0FFFFFFF;  // Mark as end of chain
				memcpy(&partition->device->buffer[entry * 4], &free_cluster, sizeof(uint32_t));
				write_buffered_sector(partition->device, partition->fat_start_sector + sector);

				uint32_t free_cluster_id = sector * 128 + entry;
				if (0 != *last_cluster) {
					uint32_t current_FAT_entry = *last_cluster * 4;
					uint32_t current_FAT_sector = partition->fat_start_sector + (current_FAT_entry / SECTOR_SIZE);  // counted from zero

					err = read_buffered_sector(partition->device, current_FAT_sector);

					uint8_t* fat_entry_buff = &partition->device->buffer[current_FAT_entry % SECTOR_SIZE];
					memcpy(fat_entry_buff, &free_cluster_id, sizeof(uint32_t));

					write_buffered_sector(partition->device, current_FAT_sector);
				}
				*last_cluster = free_cluster_id;
				err = fat32_clear_cluster(partition, &free_cluster_id);
			}
		}
	}

	return err;
}

fs_error fat32_free_cluster_chain(const fs_partition_t* partition, uint32_t* first_cluster) {
	fs_error err = FS_SUCCESS;

	uint32_t next_FAT_entry = *first_cluster;
	uint32_t current_FAT_entry = next_FAT_entry * 4;
	uint32_t current_FAT_sector = partition->fat_start_sector + (current_FAT_entry / SECTOR_SIZE);  // counted from zero

	*first_cluster = 0;

	while (FS_SUCCESS == err && next_FAT_entry != 0x0fffffff) {
		
		current_FAT_entry = next_FAT_entry * 4;
		current_FAT_sector = partition->fat_start_sector + (current_FAT_entry / SECTOR_SIZE);  // counted from zero

		err = read_buffered_sector(partition->device, current_FAT_sector);
		if (err == FS_SUCCESS) {
			uint8_t* fat_entry_buff = &get_raw_buffer(partition->device)[current_FAT_entry % SECTOR_SIZE];
			set_pending_write(partition->device);
			memcpy(&next_FAT_entry, fat_entry_buff, sizeof(uint32_t)); // Copy next cluster to be cleaned
			memset(fat_entry_buff, 0, sizeof(uint32_t));               // Clear
		}
	}

	err = write_buffered_sector(partition->device, current_FAT_sector);

	return err;
}
