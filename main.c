#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/mman.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "nrf24le1.h"

typedef enum {
	CMD_UNKNOWN,
	CMD_SHOW,
	CMD_READ_INFO,
	CMD_READ_NVM,
	CMD_READ_FIRMWARE,
	CMD_WRITE_INFO,
	CMD_WRITE_NVM,
	CMD_WRITE_FIRMWARE,
} cmd_e;

static int usage(void)
{
	fprintf(stderr, "Usage: nrf24le1 [show|read|write]\n");
	return 1;
}

static cmd_e args_to_cmd(int argc, char **argv, char **filename)
{
	int cmd_read;

	if (argc < 2)
		return CMD_UNKNOWN;

	if (strcmp(argv[1], "show") == 0)
		return CMD_SHOW;

	if (argc < 3)
		return CMD_UNKNOWN;

	if (strcmp(argv[1], "read") == 0)
		cmd_read = 1;
	else if (strcmp(argv[1], "write") == 0)
		cmd_read = 0;
	else
		return CMD_UNKNOWN;

	if (argc >= 4)
		*filename = argv[3];

	if (strcmp(argv[2], "info") == 0 || strcmp(argv[2], "infopage") == 0)
		return (cmd_read ? CMD_READ_INFO : CMD_WRITE_INFO);
	else if (strcmp(argv[2], "nvm") == 0)
		return (cmd_read ? CMD_READ_NVM : CMD_WRITE_NVM);
	else if (strcmp(argv[2], "firmware") == 0 || strcmp(argv[2], "fw") == 0)
		return (cmd_read ? CMD_READ_FIRMWARE : CMD_WRITE_FIRMWARE);

	return CMD_UNKNOWN;
}

static int filename_is_hex(char *filename)
{
	int len;

	if (filename == NULL)
		return 1;

	len = strlen(filename);
	if (len < 4)
		return 0;

	if (strcmp(filename + len - 4, ".hex") == 0)
		return 1;
	else
		return 0;
}

static void real_time_schedule(void)
{
	// Set real-time FIFO schedule to have utmost chance to have exact delays
	struct sched_param sp;
	memset(&sp, 0, sizeof(sp));
	sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &sp);

	// Make us unpageable, lock all pages to RAM with no swapping allowed
	mlockall(MCL_CURRENT | MCL_FUTURE);
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

static void save_data_hex(FILE *f, uint8_t *buf, uint16_t count)
{
	int i;

	for (i = 0; i < count; i++) {
		if ((i % 16) == 0) {
			if (i > 0)
				fprintf(f, "\n");
			fprintf(f, "%04x:", i);
		}
		fprintf(f, " %02x", buf[i]);
	}
	fprintf(f, "\n");
}

static void save_data_bin(FILE *f, uint8_t *buf, uint16_t count)
{
	fwrite(buf, 1, count, f);
}

static void save_data(char *filename, uint8_t *buf, uint16_t count)
{
	FILE *f = stdout;
	int is_hex = filename_is_hex(filename);

	if (filename) {
		f = fopen(filename, "wb");
		if (f == NULL) {
			fprintf(stderr, "Error opening file '%s', saving as hex to stdout.\n", filename);
			f = stdout;
			is_hex = 1;
		}
	} else {
		is_hex = 1;
		f = stdout;
	}

	if (is_hex)
		save_data_hex(f, buf, count);
	else
		save_data_bin(f, buf, count);
}

static void read_info(char *filename)
{
	ssize_t ret;
	uint8_t buf[NRF_PAGE_SIZE * 2];

	ret = da_infopage_show(buf);
	if (ret < 0) {
		printf("Error reading infopage, ret=%d\n", ret);
		return;
	}

	save_data(filename, buf, sizeof(buf));
}

static void read_nvm(char *filename)
{
	ssize_t ret;
	uint8_t buf[NVM_NORMAL_MEM_SIZE];

	ret = da_nvm_normal_show(buf);
	if (ret < 0) {
		printf("Error reading nvm, ret=%d\n", ret);
		return;
	}

	save_data(filename, buf, sizeof(buf));
}

int main(int argc, char **argv)
{
	uint8_t bufread[17000];
	unsigned long off = 0;
	size_t count =16384;
	cmd_e cmd;
	char *filename = NULL;

	cmd = args_to_cmd(argc, argv, &filename);
	if (cmd == CMD_UNKNOWN)
		return usage();

	memset(bufread, 0, sizeof(bufread));

	nrf24le1_init();
	real_time_schedule();

	da_enable_program_store(1);

	if (cmd == CMD_SHOW) {
		da_test_show(1);
	} else {
		// First we make sure we have proper SPI connectivity
		if (da_test_show(0) == 0) {
			// Now we run the command
			switch (cmd) {
				case CMD_WRITE_FIRMWARE:
					nrf_restore_data(bufread, count, "./firmw/blink.img");
					uhet_write(bufread, 16384, &off);
					nrf_save_data(bufread, count, "./firmw/blink-dump.img");
					break;

				case CMD_READ_FIRMWARE:
					count = uhet_read(bufread, count, &off);
					if (count > 0)
						save_data(filename, bufread, count);
					break;

				case CMD_READ_INFO:
					read_info(filename);
					break;

				case CMD_READ_NVM:
					read_nvm(filename);
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
