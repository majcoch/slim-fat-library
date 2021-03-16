/*
 * spi_master.h
 *
 * Created: 02.03.2021 14:14:23
 *  Author: Micha³ Granda
 */ 


#ifndef SPI_MASTER_H_
#define SPI_MASTER_H_

#include <avr/io.h>
#include <inttypes.h>

/* SPI master module setup */
void spi_master_init(void);
uint8_t spi_master_transfer(uint8_t byte);

/* Slave select lines management*/
void spi_slave_sd_select(uint8_t state);



#endif /* SPI_MASTER_H_ */