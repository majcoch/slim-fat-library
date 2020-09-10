#include "sd_driver.h"

#include <stddef.h>
#include "commands.h"
#include "responses.h"

#define DUMMY_BYTE 0xFF

void sd_card_set_enable(sd_card_t* sd, uint8_t enable){
	sd->spi_chip_select(enable);
}

uint8_t sd_card_tranfer_byte(sd_card_t* sd, uint8_t byte){
	return sd->spi_transfer_byte(byte);
}

void sd_card_send_command(sd_card_t* sd, uint8_t command, uint32_t argument, uint8_t CRC) {
	sd_card_tranfer_byte(sd, command);
	
	sd_card_tranfer_byte(sd, argument>>24);
	sd_card_tranfer_byte(sd, argument>>16);
	sd_card_tranfer_byte(sd, argument>>8);
	sd_card_tranfer_byte(sd, argument);
	
	sd_card_tranfer_byte(sd, CRC);
}

uint8_t sd_card_get_resp(sd_card_t* sd, uint8_t exp_bytes, uint8_t* resp_buff){
	uint8_t r1 = 0xFF;
	for(uint8_t i = 0; i < COMMAND_TIMEOUT && 0xFF == r1; i ++) {
		r1 = sd_card_tranfer_byte(sd, DUMMY_BYTE);
		/* Valid structure of response detected */
		if( !(r1 & R1_RESP_MASK) ){
			for(i = 0; i < exp_bytes; i++){
				resp_buff[i] = sd_card_tranfer_byte(sd, DUMMY_BYTE);
			}
		}
	}
	sd_card_tranfer_byte(sd, DUMMY_BYTE);	// discard response CRC
	return r1;
}

inline sd_card_err sd_card_execute_CMD0(sd_card_t* sd) {
	sd_card_err err = SD_SUCCESS;
	
	sd_card_send_command(sd, GO_IDLE_STATE, GO_IDLE_STATE_ARG, GO_IDLE_STATE_CRC);
	if(IN_IDLE_STATE != sd_card_get_resp(sd, R1_RESP_LEN, NULL)) {
		err = SD_RESET_FAIL;
		sd->type = SD_VER_UNKNOWN;
	}
	
	return err;
}

inline sd_card_err sd_card_execute_CMD8(sd_card_t* sd) {
	sd_card_err err = SD_SUCCESS;
	
	sd_card_send_command(sd, SEND_IF_COND, SEND_IF_COND_ARG, SEND_IF_COND_CRC);
	uint8_t resp_buff[R7_RESP_LEN];
	uint8_t r1 = sd_card_get_resp(sd, R7_RESP_LEN, resp_buff);
	if((r1 & R1_RESP_MASK) || !(r1 & IN_IDLE_STATE)) err = SD_TIMEOUT;
	else if( !(r1 & ILLIGAL_COMMAND) ) {
		sd->type = SD_VER_2_0_SC;
		if((resp_buff[3] != CHECK_PATTER) || (resp_buff[2] != ACCEPT_VOL_RNG))
		err = SD_UNSUPPORTED;
	}
	else sd->type = SD_VER_1_X_SC;
	
	return err;
}

inline sd_card_err sd_card_execute_CMD17(sd_card_t* sd, const uint32_t sector) {
	sd_card_err err = SD_SUCCESS;
	
	sd_card_send_command(sd, READ_SINGLE_BLOCK, sector, READ_SINGLE_BLOCK_CRC);
	uint8_t r1 = sd_card_get_resp(sd, R1_RESP_LEN, NULL);
	if(!(r1 & R1_RESP_MASK)){
		if( r1 & ADDRESS_ERROR ) err = SD_READ_ADDR_ERR;
		else if ( r1 & PARAMETER_ERROR ) err = SD_READ_OUT_RNG;
	}
	else err = SD_TIMEOUT;
	
	return err;
}

inline sd_card_err sd_card_execute_CMD58(sd_card_t* sd, uint8_t* ocr) {
	sd_card_err err = SD_SUCCESS;
	
	sd_card_send_command(sd, READ_OCR, READ_OCR_ARG, READ_OCR_CRC);
	uint8_t r1 = sd_card_get_resp(sd, R3_RESP_LEN, ocr);
	if((r1 & R1_RESP_MASK)) err = SD_TIMEOUT;
	
	return err;
}

inline sd_card_err sd_card_execute_ACMD41(sd_card_t* sd){
	sd_card_err err = SD_SUCCESS;
	
	uint8_t flag = 1;
	for(uint8_t i = 0; i < INIT_TIMEOUT && flag; i++) {
		sd_card_send_command(sd, APP_CMD, APP_CMD_ARG, APP_CMD_CRC);
		uint8_t r1 = sd_card_get_resp(sd, R1_RESP_LEN, NULL);
		if(!(r1 & R1_RESP_MASK)){
			sd_card_send_command(sd, SD_SEND_OP_COND, SD_SEND_OP_COND_ARG, SD_SEND_OP_COND_CRC);
			if(READY == sd_card_get_resp(sd, R1_RESP_LEN, NULL)) flag = 0;
		}
		else {
			flag = 0;
			err = SD_TIMEOUT;
		}
	}
	if(flag) err = SD_INIT_FAIL;
	
	return err;
}

inline sd_card_err sd_card_await_read(sd_card_t* sd){
	sd_card_err err = SD_SUCCESS;
	
	uint8_t flag = 1;
	for (uint8_t timeout = 0; timeout < ACCESS_TIMEOUT && flag; timeout++) {
		uint8_t resp = sd_card_tranfer_byte(sd, DUMMY_BYTE);
		if( BLOCK_START_TOKEN == resp ) flag = 0;
	}
	if(flag) err = SD_READ_FAIL;
	
	return err;
}



sd_card_err sd_card_init(sd_card_t* sd) {
	sd_card_err err = SD_SUCCESS;
	
	/* Sending at least 74 dummy clock cycles prior to initialization */
	sd_card_set_enable(sd, SD_DISABLE);
	for(uint8_t counter = 0; counter < RESET_TIMEOUT; counter++) {
		sd_card_tranfer_byte(sd, DUMMY_BYTE);
	}
	/* Initialization */
	sd_card_set_enable(sd, SD_ENABLE);
	// Reset interface - early exit on fail
	err = sd_card_execute_CMD0(sd);
	if(SD_SUCCESS == err){
		uint8_t sd_ocr[R3_RESP_LEN];
		err = sd_card_execute_CMD8(sd);
		if(SD_SUCCESS == err)
			err = sd_card_execute_CMD58(sd, sd_ocr);
		if(SD_SUCCESS == err)
			err = (sd_ocr[1] & VDD_VOLTAGE) ? SD_SUCCESS : SD_UNSUPPORTED;
		if(SD_SUCCESS == err)
			err = sd_card_execute_ACMD41(sd);
		if(SD_SUCCESS == err && sd->type == SD_VER_2_0_SC) {
			err = sd_card_execute_CMD58(sd, sd_ocr);
			if(SD_SUCCESS == err){
				if(!(sd_ocr[0] & POW_UP_STAT))
					err = SD_INIT_FAIL;
				else if(sd_ocr[0] & CARD_CAPACITY)
					sd->type = SD_VER_2_0_HC;
			}
		}
	}
	
	sd_card_set_enable(sd, SD_DISABLE);
	return err;
}

sd_card_err sd_card_read(sd_card_t* sd, const uint32_t sector, uint8_t* buffer) {
	sd_card_err err = SD_SUCCESS;
	
	uint32_t sector_to_read = sector;
	if(sd->type != SD_VER_2_0_HC) sector_to_read <<= 9;
	
	sd_card_set_enable(sd, SD_ENABLE);
	err = sd_card_execute_CMD17(sd, sector_to_read);
	if(SD_SUCCESS == err) {
		// Wait for data start token
		err = sd_card_await_read(sd);
		if(SD_SUCCESS == err) {
			// Get sector data
			for(uint16_t count = 0; count < SECTOR_SIZE; count++)
			buffer[count] = sd_card_tranfer_byte(sd, DUMMY_BYTE);
			// Get CRC
			sd_card_tranfer_byte(sd, DUMMY_BYTE);
			sd_card_tranfer_byte(sd, DUMMY_BYTE);
		}
	}
	
	sd_card_set_enable(sd, SD_DISABLE);
	return err;
}
