/*
 * responses.h
 *
 * Created: 05.09.2020 12:21:56
 * Author : Micha� Granda
 */ 


#ifndef RESPONSES_H_
#define RESPONSES_H_


#define R1_RESP_LEN			0
#define R1_RESP_MASK		0b10000000
#define READY				0b00000000
#define IN_IDLE_STATE		0b00000001
#define ERASE_RESET			0b00000010
#define ILLIGAL_COMMAND		0b00000100
#define COM_CRC_ERROR		0b00001000
#define ERASE_SEQ_ERROR		0b00010000
#define ADDRESS_ERROR		0b00100000
#define PARAMETER_ERROR		0b01000000

#define R2_RESP_LEN			1

#define R3_RESP_LEN			4
#define VDD_VOLTAGE			0b00001000
#define POW_UP_STAT			0b10000000
#define CARD_CAPACITY		0b01000000

#define R7_RESP_LEN			4
#define CHECK_PATTER		0xAA
#define ACCEPT_VOL_RNG		0b0001


#endif /* RESPONSES_H_ */