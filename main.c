#include <wiring.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "nrf24le1.h"

int main(void) {
	uint8_t buffer[] = {0xFF, 0xFF};

	printf("Inicializando wiringPi\n");

	wiring_init();

	enable_program(1);

	da_test_show();

	enable_program(0);
	
  	return 0;
}
