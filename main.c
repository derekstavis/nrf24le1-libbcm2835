//#include <wiring.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "nrf24le1.h"

void nrf_save_data(uint8_t * buf, uint16_t count, uint8_t * fname)
{
	size_t size_writed = 0, idx = 0;
	FILE * fd;

	fd = fopen(fname, "w");

	while(idx < count)
	{
		size_writed = fwrite(&buf[idx], 1, (count - idx), fd);
		idx = size_writed + idx;
	}
	fclose(fd);
}

void nrf_restore_data(uint8_t * buf, uint16_t count, uint8_t * fname)
{
	FILE * fd;
	fd = fopen(fname, "r");
	fread(buf, 1, count, fd);
	fclose(fd);
}
int main(void) {
	uint8_t bufread[17000];
	unsigned long off = 0;
	size_t count =16384;

	memset(bufread, 0, sizeof(bufread));

	nrf24le1_init();

	enable_program(1);

	//da_test_show();
#if 1
	nrf_restore_data(bufread, count, "./firmw/blink.img");
	uhet_write(bufread, 16384, &off);
	nrf_save_data(bufread, count, "./firmw/blink-dump.img");
#endif

#if 1
	memset(bufread, 0, sizeof(bufread));
	uhet_read(bufread, count, &off);

	nrf_save_data(bufread, count, "./blink-dump2.img");
#endif

	//da_erase_all_store();

	enable_program(0);

	wiring_destroy();
	
  	return 0;
}
