#include "spi_master.h"

void spi_master_init(void) {
	// SPI hardware initialization
	DDRB |= (1<<PORTB3)|(1<<PORTB5)|(1<<PORTB2);
	PORTB |= (1<<PORTB2);	// set SS high
	SPCR |= (1<<SPE)|(1<<MSTR);
	SPSR |= (1<<SPI2X);
	
	// Slave Select lines initialization
	DDRB |= (1<<PORTB1);
	PORTB |= (1<<PORTB1);
}

uint8_t spi_master_transfer(uint8_t byte) {
	SPDR = byte;
	while(!(SPSR & (1<<SPIF)));
	return SPDR;
}

void spi_slave_sd_select(uint8_t state) {
	if(state) PORTB |= (1<<PORTB1);
	else PORTB &= ~(1<<PORTB1);
}
