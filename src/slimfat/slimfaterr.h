/*
 * slimfaterr.h
 *
 * Created: 24.09.2020 17:24:14
 * Author : Micha³ Granda
 */


#ifndef SLIMFATERR_H_
#define SLIMFATERR_H_

typedef enum {
	FS_SUCCESS,
	FS_READ_FAIL,
	FS_WRITE_FAIL,
	FS_SIG_MISMATCH
} fs_error;

#endif /* SLIMFATERR_H_ */