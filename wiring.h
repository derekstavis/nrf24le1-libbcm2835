#ifndef __WIRING_H__
#define __WIRING_H__

#include <wiringPi.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* SPI Channel and Speed Config */
#define WIRING_SPI_CHANNEL 		0

/* RaspberryPi Pinout Config */
#define WIRING_SPI_MOSI_PIN		12
#define WIRING_SPI_MISO_PIN		13
#define WIRING_SPI_CLK_PIN		14
#define WIRING_SPI_CS0_PIN		10

/* nrf24LE1 required signals */
#define WIRING_NRF_PROG_PIN		5
#define WIRING_NRF_RESET_PIN	6

/* Macros for sleep happiness */
#define udelay(us)		delayMicroseconds(us)
#define mdelay(ms)		delayMicroseconds(ms*1000)

/* Wiring functions for bootstraping SPI */
void wiring_init(unsigned);

/* Full-duplex read and write function */
uint8_t wiring_write_then_read(uint8_t* in, 
	                           uint8_t in_len, 
	                   		   uint8_t* out, 
	                   		   uint8_t out_len);

/* Function for setting gpio values */
void wiring_set_gpio_value(uint8_t pin, uint8_t state);

#endif