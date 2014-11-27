#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "nrf24le1.h"

typedef enum {
	CMD_UNKNOWN,
	CMD_SHOW,
	CMD_READ,
	CMD_WRITE,
} cmd_e;

static int usage(void)
{
	fprintf(stderr, "Usage: nrf24le1 [show|read|write]\n");
	return 1;
}

static cmd_e arg_to_cmd(const char *arg)
{
	if (strcmp(arg, "show") == 0)
		return CMD_SHOW;
	else if (strcmp(arg, "read") == 0)
		return CMD_READ;
	else if (strcmp(arg, "write") == 0)
		return CMD_WRITE;
	else
		return CMD_UNKNOWN;
}

static void nrf_save_data(uint8_t * buf, uint16_t count, char * fname)
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

static void nrf_restore_data(uint8_t * buf, uint16_t count, char * fname)
{
	FILE * fd;
	fd = fopen(fname, "r");
	fread(buf, 1, count, fd);
	fclose(fd);
}

int main(int argc, char **argv)
{
	uint8_t bufread[17000];
	unsigned long off = 0;
	size_t count =16384;
	cmd_e cmd;

	if (argc < 2) {
		return usage();
	}
	cmd = arg_to_cmd(argv[1]);

	memset(bufread, 0, sizeof(bufread));

	nrf24le1_init();

	da_enable_program_store(1);

	if (cmd == CMD_SHOW) {
		da_test_show(1);
	} else {
		// First we make sure we have proper SPI connectivity
		if (da_test_show(0) == 0) {
			// Now we run the command
			switch (cmd) {
				case CMD_WRITE:
					nrf_restore_data(bufread, count, "./firmw/blink.img");
					uhet_write(bufread, 16384, &off);
					nrf_save_data(bufread, count, "./firmw/blink-dump.img");
					break;

				case CMD_READ:
					memset(bufread, 0, sizeof(bufread));
					uhet_read(bufread, count, &off);
					nrf_save_data(bufread, count, "./blink-dump2.img");
					break;

				default:
					break;
			}
		}
	}

	da_enable_program_store(0);

	nrf24le1_cleanup();
	
  	return 0;
}
