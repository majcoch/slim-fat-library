/*
 * commands.h
 *
 * Created: 05.09.2020 12:21:13
 * Author : Micha³ Granda
 */ 


#ifndef COMMANDS_H_
#define COMMANDS_H_


#define GO_IDLE_STATE			0x40
#define GO_IDLE_STATE_ARG		0x00000000
#define GO_IDLE_STATE_CRC		0x95

#define SEND_IF_COND			0x48
#define SEND_IF_COND_ARG		0x000001AA
#define SEND_IF_COND_CRC		0x87

#define READ_OCR				0x7A
#define READ_OCR_ARG			0x00000000
#define READ_OCR_CRC			0x00

#define APP_CMD					0x77
#define APP_CMD_ARG				0x00000000
#define APP_CMD_CRC				0x00

#define SD_SEND_OP_COND			0x69
#define SD_SEND_OP_COND_ARG		0x40000000	// SD HC accepted
#define SD_SEND_OP_COND_CRC		0x00

#define SEND_STATUS				0x4D
#define SEND_STATUS_ARG			0x00000000
#define SEND_STATUS_CRC			0x00

#define READ_SINGLE_BLOCK		0x51
#define READ_SINGLE_BLOCK_CRC	0x00

#define WRITE_BLOCK				0x58
#define WRITE_BLOCK_CRC			0x00


#endif /* COMMANDS_H_ */