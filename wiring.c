#include "wiring.h"

/* SPI Channel and Speed Config */
#define SPI_CHANNEL 	0
#define SPI_SPEED		2000000

/* RaspberryPi Pinout Config */
#define SPI_MOSI_PIN	12
#define SPI_MISO_PIN	13
#define SPI_CLK_PIN		14
#define SPI_CS0_PIN		10

/* nrf24LE1 required signals */
#define NRF_PROG_PIN	5
#define NRF_RESET_PIN	6

void
wiring_init() 
{

	if (wiringPiSetup() < 0) {
		printf("wiringPi Setup failed\n");
		exit(errno);
	}

	if (wiringPiSPISetup (SPI_CHANNEL, SPI_SPEED) < 0) {
  		printf ("SPI Setup failed\n");
  		exit(errno);
  	} else {
  		pinMode(NRF_PROG_PIN, OUTPUT);
  		pinMode(NRF_RESET_PIN, OUTPUT);
  	}

}

uint8_t
wiring_write_then_read(uint8_t* in, uint8_t in_len, 
	                   uint8_t* out, uint8_t out_len)
{
	wiringPiSPIDataRW(in, in_len);

	return 0;
}

void
wiring_set_gpio_value(uint8_t pin, uint8_t state)
{
	digitalWrite (pin, state) ;
}