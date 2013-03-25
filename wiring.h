#ifndef __WIRING_H__
#define __WIRING_H__

#include <wiringPi.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define udelay(us)		delayMicroseconds(us)
#define mdelay(ms)		delayMicroseconds(ms*1000)

void wiring_init();

uint8_t wiring_write_then_read(uint8_t* in, 
	                           uint8_t in_len, 
	                   		   uint8_t* out, 
	                   		   uint8_t out_len);

void wiring_set_gpio_value(uint8_t pin, uint8_t state);

#endif