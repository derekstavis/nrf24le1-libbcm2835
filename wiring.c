#include "wiring.h"
#include <string.h>

void
wiring_init() 
{

	if (!bcm2835_init()) {
		printf("bcm2835 init failed\n");
		exit(errno);
	}


	bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      // The default
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   // The default
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64);    // 4Mhz clock
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      // The default
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);      // the default

	
	bcm2835_gpio_fsel(WIRING_NRF_PROG_PIN, BCM2835_GPIO_FSEL_OUTP);	
	bcm2835_gpio_fsel(WIRING_NRF_RESET_PIN, BCM2835_GPIO_FSEL_OUTP);

}

uint8_t
wiring_write_then_read(uint8_t* out, uint8_t out_len, 
	               uint8_t* in, uint8_t in_len)
{
	uint8_t transfer_buf[out_len + in_len];
	unsigned int ret = 0;

	memset(transfer_buf, 0, out_len + in_len);
	
	if (NULL != out) {
		memcpy(transfer_buf, out, out_len);
		ret += out_len;
	}

	if (NULL != in) {
		ret += in_len;
	}

	bcm2835_spi_transfern(transfer_buf, ret);

	memcpy(in, &transfer_buf[out_len], in_len);

	return ret;
}

void
wiring_set_gpio_value(uint8_t pin, uint8_t state)
{
	bcm2835_gpio_write(pin, state);
}

void
wiring_destroy()
{
	bcm2835_spi_end();
}
