#include "storage.h"

#include <string.h>

uint8_t validate_signature(uint8_t* buffer) {
	return (buffer[510] != 0x55 || buffer[511] != 0xAA);
}

fs_error find_partition(fs_storage_device* device, const uint8_t partition_number, uint32_t* sector) {
	fs_error err = FS_SUCCESS;

	err = read_buffered_sector(device, 0);	// Expected to see MBR (Master Boot Record)
	if (FS_SUCCESS == err) {
		if (!validate_signature(device->buffer)) {
			uint8_t* partition_entry = &device->buffer[0x01BE + (partition_number * 16)];
			memcpy(sector, &partition_entry[8], sizeof(uint32_t));
		}
		else {
			err = FS_SIG_MISMATCH;
		}
	}

	return err;
}

fs_error read_buffered_sector(fs_storage_device* device, const uint32_t sector) {
	fs_error err = FS_SUCCESS;

	if (device->sector != sector || 0 == sector) {
		if (1 == device->status) {
			device->status = 0;	// Make sure this is clear after successful write
			if (device->write_sector(device->disk, device->sector, device->buffer)) {
				err = FS_WRITE_FAIL;
			}
		}
		device->sector = sector;
		if (device->read_sector(device->disk, sector, device->buffer)) {
			err = FS_READ_FAIL;
		}
	}

	return err;
}

fs_error write_buffered_sector(fs_storage_device* device, const uint32_t sector) {
	fs_error err = FS_SUCCESS;

	device->status = 0;	// Make sure this is clear after successful write
	device->sector = sector;
	if (device->write_sector(device->disk, sector, device->buffer)) {
		err = FS_WRITE_FAIL;
	}

	return err;
}

uint8_t* get_raw_buffer(fs_storage_device* device) {
	return device->buffer;
}

void set_pending_write(fs_storage_device* device) {
	device->status = 1;
}
