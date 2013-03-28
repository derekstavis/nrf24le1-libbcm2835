#ifndef __WIRING_H__
#define __WIRING_H__

#include <bcm2835.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* nrf24LE1 required signals */
#define WIRING_NRF_PROG_PIN		24
#define WIRING_NRF_RESET_PIN	25

/* Macros for sleep happiness */
#define udelay(us)		bcm2835_delayMicroseconds(us)
#define mdelay(ms)		bcm2835_delayMicroseconds(ms*1000)

/* Wiring functions for bootstraping SPI */
void wiring_init();

/* Full-duplex read and write function */
uint8_t wiring_write_then_read(uint8_t* in, 
	                           uint8_t in_len, 
	                   		   uint8_t* out, 
	                   		   uint8_t out_len);

/* Function for setting gpio values */
void wiring_set_gpio_value(uint8_t pin, uint8_t state);

#endif