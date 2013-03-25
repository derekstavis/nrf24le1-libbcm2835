#include "wiring.h"
#include <string.h>

void
wiring_init(unsigned spi_speed) 
{

	if (wiringPiSetup() < 0) {
		printf("wiringPi Setup failed\n");
		exit(errno);
	}

	if (wiringPiSPISetup (WIRING_SPI_CHANNEL, spi_speed) < 0) {
  		printf ("SPI Setup failed\n");
  		exit(errno);
  	} else {
  		pinMode(WIRING_NRF_PROG_PIN, OUTPUT);
  		pinMode(WIRING_NRF_RESET_PIN, OUTPUT);
  	}

}

uint8_t
wiring_write_then_read(uint8_t* in, uint8_t in_len, 
	                   uint8_t* out, uint8_t out_len)
{
	uint8_t out_buf[out_len];
	uint8_t in_buf[in_len];
	unsigned int ret = 0;
	
	if (NULL != out) {
		memcpy(out_buf, out, out_len);
		wiringPiSPIDataRW(WIRING_SPI_CHANNEL, out_buf, out_len);
		ret += out_len;
	}


	if (NULL != in) {
		memcpy(in_buf, in, in_len);		
		wiringPiSPIDataRW(WIRING_SPI_CHANNEL, in_buf, in_len);
		ret += in_len;
	}

	return ret;
}

void
wiring_set_gpio_value(uint8_t pin, uint8_t state)
{
	digitalWrite (pin, state) ;
}