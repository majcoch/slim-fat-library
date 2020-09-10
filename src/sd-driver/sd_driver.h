/*
 * sd_driver.h
 *
 * Created: 04.09.2020 10:11:06
 * Author : Micha³ Granda
 */ 


#ifndef SD_DRIVER_H_
#define SD_DRIVER_H_

#include <stdint.h>

#define SECTOR_SIZE 512

/* ---------- SD CARD CHIP SELECT ---------- */
#define SD_ENABLE	0
#define SD_DISABLE	1

/* ------------ SD CARD TIMINGS ------------ */
#define RESET_TIMEOUT	10
#define INIT_TIMEOUT	200
#define COMMAND_TIMEOUT	100
#define ACCESS_TIMEOUT	200

typedef enum {
	SD_SUCCESS,
	SD_TIMEOUT,			// When card fails to respond with valid data for command request
	SD_RESET_FAIL,		// When card fails to go idle state - card may not be present in slot	
	SD_INIT_FAIL,		// When card was unable to initialize
	SD_UNSUPPORTED,		// When card cannot be recognized or operating conditions are not met
	SD_READ_FAIL,		// When card was unable to send data token and start transmitting data
	SD_READ_ADDR_ERR,	// Reading from unaligned sector address
	SD_READ_OUT_RNG,	// Reading outside of card address range
}sd_card_err;

typedef enum{
	SD_VER_UNKNOWN,
	SD_VER_1_X_SC,
	SD_VER_2_0_SC,
	SD_VER_2_0_HC
} sd_card_type;

typedef struct {
	sd_card_type type;
	uint8_t (*spi_transfer_byte)(uint8_t);
	void (*spi_chip_select)(uint8_t);
} sd_card_t;

sd_card_err	sd_card_init(sd_card_t* sd);
sd_card_err	sd_card_read(sd_card_t* sd, const uint32_t sector, uint8_t* buffer);
sd_card_err	sd_card_write(sd_card_t* sd, const uint32_t sector, const uint8_t* buffer);

#endif /* SD_DRIVER_H_ */