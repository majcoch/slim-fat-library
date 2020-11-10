/*
 * fat32.h
 *
 * Created: 06.10.2020 21:09:44
 * Author : Micha³ Granda
 */


#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include "../slimfaterr.h"
#include "../storage/storage.h"

typedef struct fs_fat32_partition {
	// Hardware device on which partition exists
	fs_storage_device* device;
	// FAT32 partition data
	uint16_t bytes_per_sector;
	uint8_t	 sectors_per_cluster;
	uint32_t root_cluster;
	uint32_t sectors_pre_fat;
	uint32_t fat_start_sector;
	uint32_t data_start_sector;
} fs_partition_t;

typedef struct fat32_entry {
	// Basic file info
	uint8_t  attributes;
	uint32_t file_size;
	uint32_t starting_cluster;
	// File access variables
	uint32_t root_dir_cluster;
	uint16_t root_dir_offset;
} fat_entry_t;

/* Partition operation */
fs_error fat32_mount_partition(fs_partition_t* partition, const uint32_t start_sector);

/* File entry operations */
fs_error fat32_find_entry(const fs_partition_t* partition, fat_entry_t* file, const uint8_t* name, const uint8_t name_len);
fs_error fat32_create_entry(const fs_partition_t* partition, fat_entry_t* entry, const uint8_t* name, const uint8_t name_len);
fs_error fat32_update_entry(const fs_partition_t* partition, fat_entry_t* file);

/* Cluster operations */
uint32_t fat32_get_cluster_sector(const fs_partition_t* partition, const uint32_t* cluster);

fs_error fat32_find_next_cluster(const fs_partition_t* partition, uint32_t* cluster);
fs_error fat32_alloc_new_cluster(const fs_partition_t* partition, uint32_t* last_cluster);
fs_error fat32_free_cluster_chain(const fs_partition_t* partition, uint32_t* first_cluster);

#endif