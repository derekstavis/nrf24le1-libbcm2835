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

	if (strcmp(filename + len - 4, ".hex") == 0 ||
            strcmp(filename + len - 4, ".ihx") == 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
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

static void save_line_hex(FILE *f, uint16_t addr, uint8_t *buf, uint16_t count)
{
	uint16_t i;
	uint16_t to_skip_bytes = 0;
	uint16_t to_save_bytes = 0;
	uint8_t checksum = 0;

	/* We want to save the shortest line, we ignore 0xFF parts at the start
 	 * and end of the buffer */
	for (i = 0; i < count; i++) {
		if (buf[i] != 0xFF)
			to_save_bytes = i + 1;
		else if (buf[i] == 0xFF && to_save_bytes == 0)
			to_skip_bytes = i + 1;
	}

	if (to_save_bytes == 0)
		return; // Nothing to save

	if (to_save_bytes - to_skip_bytes > count / 2) {
		// Unless we saved half the line, just keep form and save the entire line
		to_save_bytes = 32;
		to_skip_bytes = 0;
	}

	fprintf(f, ":%02X%04X00", (uint8_t)(to_save_bytes - to_skip_bytes), addr + to_skip_bytes);
	checksum += to_save_bytes - to_skip_bytes;
	checksum += (addr + to_skip_bytes) >> 8;
	checksum += (addr + to_skip_bytes) & 0xFF;
	for (i = to_skip_bytes; i < to_save_bytes; i++) {
		fprintf(f, "%02X", buf[i]);
		checksum += buf[i];
	}
	fprintf(f, "%02X\n", (uint8_t)((~checksum) + 1));
}

static void save_data_hex(FILE *f, uint8_t *buf, uint16_t count)
{
	int i;

	for (i = 0; i < count; i += 32) {
		uint16_t left = 32;
		if (count - i < 32)
			left = count - i;
		save_line_hex(f, i, buf+i, left);
	}
	fprintf(f, ":00000001FF\n"); // EOF
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

static uint16_t read_data_bin(FILE *f, uint8_t *buf, uint16_t size)
{
	return fread(buf, 1, size, f);
}

static int parse_hex(char *line, uint8_t *data)
{
	int i;
	int first_nibble = 1;
	uint8_t nibble;

	// Start after the colon ':'
	for (i = 1; line[i] != 0 && line[i] != '\n' && line[i] != '\r'; i++) {
		if (line[i] >= '0' && line[i] <= '9')
			nibble = line[i] - '0';
		else if (line[i] >= 'a' && line[i] <= 'f')
			nibble = line[i] - 'a' + 0xA;
		else if (line[i] >= 'A' && line[i] <= 'F')
			nibble = line[i] - 'A' + 0xA;
		else {
			// Invalid hex character
			return i;
		}

		if (first_nibble) {
			*data = nibble << 4;
			first_nibble = 0;
		} else {
			*data |= nibble;
			first_nibble = 1;
			data++;
		}
	}

	if (!first_nibble)
		return i;

	return -1;
}

static uint16_t read_data_hex(FILE *f, uint8_t *buf, uint16_t size)
{
	char line[1024]; // Maximum likely size is 1 + 2 + 4 + 2 + 255*2 + 2 + 2 = 523
	uint8_t data[512];
	int i;
	uint16_t max_data = 0;
	uint8_t num_data;
	uint8_t num_bytes;
	uint16_t addr;
	uint8_t rec_type;
	uint8_t checksum;

	memset(buf, 0xFF, size);

	while (!feof(f) && !ferror(f)) {
		if (fgets(line, sizeof(line), f) == NULL)
			break;
		line[sizeof(line)-1] = 0;

		if (line[0] != ':') {
			fprintf(stderr, "Invalid data in hex file (Intel Hex requires a colon at start)\n");
			return 0;
		}

		int err_index = parse_hex(line, data);
		if (err_index >= 0) {
			fprintf(stderr, "Error parsing hex at character %d\n", err_index);
			fprintf(stderr, "Line: '%s'\n", line);
			fprintf(stderr, "Err :  ");
			for (i = 0; i < err_index; i++)
				fprintf(stderr, " ");
			fprintf(f, "^\n");
			return 0;
		}
		num_data = data[0] + 5;
		num_bytes = data[0];
		addr = (data[1] << 8) | data[2];
		rec_type = data[3];
		checksum = 0;
		for (i = 0; i < num_data; i++)
			checksum += data[i];
		if (checksum != 0) {
			fprintf(stderr, "Incorrect checksum in line: '%s'\n", line);
			return 0;
		}
		switch (rec_type) {
			case 0:
				if (addr + num_bytes > size) {
					fprintf(stderr, "Data is larger than allowed buffer\n");
					return 0;
				}
				memcpy(buf + addr, data + 4, num_bytes);
				if (addr + num_bytes > max_data)
					max_data = addr + num_bytes;
				printf("Processed %u bytes at address %u, new max data is %u\n", num_bytes, addr, max_data);
				break;
			case 1:
				fprintf(stderr, "EOF\n");
				goto Exit;
			default:
				fprintf(stderr, "Unknown record type %d\n", rec_type);
				return 0;
		}
	}

Exit:
	return max_data;
}

static uint16_t read_data(char *filename, uint8_t *buf, uint16_t size)
{
	FILE *f = stdout;
	int is_hex = filename_is_hex(filename);

	if (filename) {
		f = fopen(filename, "rb");
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
		return read_data_hex(f, buf, size);
	else
		return read_data_bin(f, buf, size);
}

static void read_info(char *filename)
{
	ssize_t ret;
	uint8_t buf[NRF_PAGE_SIZE];

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
	uint8_t bufread[16*1024];
	unsigned long off = 0;
	size_t count = sizeof(bufread);
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
					count = read_data(filename, bufread, sizeof(bufread));
					if (count > 0)
						uhet_write(bufread, count, &off);
					else
						fprintf(stderr, "Failed to read data to program\n");
					break;

				case CMD_WRITE_INFO:
					count = read_data(filename, bufread, INFO_PAGE_SIZE);
					if (count > 0)
						da_infopage_store(bufread, count);
					else
						fprintf(stderr, "Failed to read data to program\n");
					break;

				case CMD_WRITE_NVM:
					count = read_data(filename, bufread, NVM_NORMAL_MEM_SIZE);
					if (count > 0)
						da_nvm_normal_store(bufread, count);
					else
						fprintf(stderr, "Failed to read data to program\n");
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
