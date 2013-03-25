#include <wiring.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "nrf24le1.h"

int main(void) {
	
	nrf24le1_init();

	enable_program(1);

	da_test_show();

	enable_program(0);
	
  	return 0;
}
