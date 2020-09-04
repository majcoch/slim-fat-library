/*
 * sd_driver.h
 *
 * Created: 04.09.2020 10:11:06
 * Author : Micha³ Granda
 */ 


#ifndef SD_DRIVER_H_
#define SD_DRIVER_H_

#include <stdint.h>

typedef enum {
}sd_card_err;

typedef enum{
} sd_card_type;

typedef struct {
} sd_card_t;


sd_card_err	sd_card_init(sd_card_t* sd);
sd_card_err	sd_card_read(sd_card_t* sd, const uint32_t sector, uint8_t* buffer);
sd_card_err	sd_card_write(sd_card_t* sd, const uint32_t sector, const uint8_t* buffer);


#endif /* SD_DRIVER_H_ */