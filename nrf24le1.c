#ifndef __NRF24LE1_H__
#define __NRF24LE1_H__

#include "nrf24le1.h"
#include "wiring.h"

#include <endian.h>

#define NAME "nrf24le1"

void uhet_record_init(void);
void uhet_record_end(void);

void _erase_all(void);
void _erase_page(unsigned i);
void _erase_program_pages(void);

int _enable_program = 0;

#define write_then_read(out,n_out,in,n_in) \
({ \
	int __ret = 0; \
\
	__ret = wiring_write_then_read(out,n_out,in,n_in); \
	if (0 > __ret){ \
		debug("falha na operacao de write_then_read"); \
	} \
\
	__ret; \
})

void nrf24le1_init()
{
	debug("Inicializando nRF24LE1\n");
	wiring_init();
}

void nrf24le1_cleanup(void)
{
	wiring_destroy();
}

void _wait_for_ready(void)
{
	uint8_t cmd[1] = { SPICMD_RDSR };
	uint8_t fsr[1] = { 0xAA };
	int count = 0;

	do {
		if (count == 1000) {
			debug("nao posso esperar pra sempre mano, FSR: 0x%X",
			      *fsr);
			return;
		}
		count++;

		wiring_write_then_read(cmd, 1, fsr, 1);
		udelay(500);

	} while (*fsr & FSR_RDYN);
}

int _enable_infopage_access(void)
{
	uint8_t cmd[2];
	uint8_t in[1];
	uint8_t fsr_orig;

	// read fsr
	cmd[0] = SPICMD_RDSR;
	write_then_read(cmd, 1, in, 1);
	fsr_orig = in[0];

	// fsr.INFEN = 1
	cmd[0] = SPICMD_WRSR;
	cmd[1] = fsr_orig | FSR_INFEN;
	write_then_read(cmd, 2, NULL, 0);

	// read fsr
	cmd[0] = SPICMD_RDSR;
	write_then_read(cmd, 1, in, 1);

	// comparando escrita
	if (in[0] != (fsr_orig | FSR_INFEN)) {
		debug("falha em habilitar acesso a infopage %X %X", fsr_orig,
		      in[0]);
		return -EINVAL;
	}

	return 0;
}

int _read_infopage(uint8_t *buf)
{
	int i;
	int ret = 0;

	uint8_t cmd[3];
	uint8_t in[N_BYTES_FOR_READ];

	uint16_t *addr = (uint16_t *) (cmd + 1);
	uint8_t *p = buf;

	cmd[0] = SPICMD_READ;
	for (i = 0; i < NRF_PAGE_SIZE; i += N_BYTES_FOR_READ) {

		*addr = htole16(i);
		ret = write_then_read(cmd, 3, in, N_BYTES_FOR_READ);
		if (0 != ret)
			return ret;

		memcpy(p, in, N_BYTES_FOR_READ);

		p += N_BYTES_FOR_READ;
	}

	debug("numero de bytes lidos: %i\n", p - buf);

	return p - buf;		// numero de bytes lidos;
}

int _read_nvm_normal(uint8_t *buf)
{
	int i;
	int ret = 0;

	uint8_t cmd[3];
	uint8_t in[N_BYTES_FOR_READ];

	uint16_t *addr = (uint16_t *) (cmd + 1);
	uint8_t *p = buf;

	cmd[0] = SPICMD_READ;
	for (i = 0; i < NVM_NORMAL_MEM_SIZE; i += N_BYTES_FOR_READ) {

		*addr = htole16(i + NVM_NORMAL_PAGE0_INI_ADDR);
		ret = write_then_read(cmd, 3, in, N_BYTES_FOR_READ);
		if (0 != ret)
			return ret;

		memcpy(p, in, N_BYTES_FOR_READ);

		p += N_BYTES_FOR_READ;
	}

	debug("numero de bytes lidos: %i\n", p - buf);

	return p - buf;		// numero de bytes lidos;
}

int _disable_infopage_access(void)
{
	uint8_t cmd[2];
	uint8_t in[1];
	uint8_t fsr_orig;

	// read fsr
	cmd[0] = SPICMD_RDSR;
	write_then_read(cmd, 1, in, 1);
	fsr_orig = in[0];

	// fsr.INFEN = 0
	cmd[0] = SPICMD_WRSR;
	cmd[1] = fsr_orig & ~(FSR_INFEN);
	write_then_read(cmd, 2, NULL, 0);

	// read fsr
	cmd[0] = SPICMD_RDSR;
	write_then_read(cmd, 1, in, 1);

	// comparando escrita
	if (in[0] != (fsr_orig & ~(FSR_INFEN))) {
		debug("falha em desabilitar acesso a infopage %X %X",
		      fsr_orig, in[0]);
		return -EINVAL;
	}

	return 0;
}

ssize_t
da_enable_program_show()
{
	printf("%i\n", _enable_program);
	return 0;
}

ssize_t
da_enable_program_store(uint8_t state)
{
	int ret = 0;

	if (state != 0 && state != 1) {
		ret = -EINVAL;
		goto end;
	}

	switch (state) {

	case 0:
		switch (_enable_program) {
		case 0:
			goto end;

		case 1:
			_enable_program = 0;
			uhet_record_end();
			goto end;
		}

	case 1:
		switch (_enable_program) {
		case 1:
			goto end;

		case 0:
			_enable_program = 1;
			uhet_record_init();
			_wait_for_ready();
			goto end;
		}
	}

end:
	return ret;
}

ssize_t
da_erase_all_store()
{
	int ret = 0;

	if (0 == _enable_program) {
		debug("falha, enable_program = 0");
		ret = -EINVAL;
		goto end;
	}

	_erase_all();

end:
	return ret;
}

ssize_t
da_test_show()
{
	uint8_t cmd;
	uint8_t fsr;
	int ret = 0;

	if (0 == _enable_program) {
		debug("falha, enable_program = 0");
		ret = -EINVAL;
		goto end;
	}

	cmd = SPICMD_RDSR;
	write_then_read(&cmd, 1, &fsr, 1);
	ret += printf("* FSR original\n");
	ret += printf("-> FSR.RDISMB: %i\n",  (fsr & FSR_RDISMB ? 1 : 0));
	ret += printf("-> FSR.INFEN: %i\n",   (fsr & FSR_INFEN ? 1 : 0));
	ret += printf("-> FSR.RDYN: %i\n",    (fsr & FSR_RDYN ? 1 : 0));
	ret += printf("-> FSR.WEN: %i\n",     (fsr & FSR_WEN ? 1 : 0));
	ret += printf("-> FSR.STP: %i\n",     (fsr & FSR_STP ? 1 : 0));
	ret += printf("-> FSR.ENDEBUG: %i\n", (fsr & FSR_ENDEBUG ? 1 : 0));

	cmd = SPICMD_WREN;
	write_then_read(&cmd, 1, NULL, 0);

	cmd = SPICMD_RDSR;
	write_then_read(&cmd, 1, &fsr, 1);
	ret += printf("* FSR apos WREN, WEN deve ser 1\n");
	ret += printf("-> FSR.RDISMB: %i\n", (fsr & FSR_RDISMB ? 1 : 0));
	ret += printf("-> FSR.INFEN: %i\n", (fsr & FSR_INFEN ? 1 : 0));
	ret += printf("-> FSR.RDYN: %i\n", (fsr & FSR_RDYN ? 1 : 0));
	ret += printf("-> FSR.WEN: %i\n", (fsr & FSR_WEN ? 1 : 0));
	ret += printf("-> FSR.STP: %i\n", (fsr & FSR_STP ? 1 : 0));
	ret += printf("-> FSR.ENDEBUG: %i\n", (fsr & FSR_ENDEBUG ? 1 : 0));

	cmd = SPICMD_WRDIS;
	write_then_read(&cmd, 1, NULL, 0);

	cmd = SPICMD_RDSR;
	write_then_read(&cmd, 1, &fsr, 1);
	ret += printf("* FSR apos WRDIS, WEN deve ser 0\n");
	ret +=
	    printf("-> FSR.RDISMB: %i\n",
		    (fsr & FSR_RDISMB ? 1 : 0));
	ret +=
	    printf("-> FSR.INFEN: %i\n", (fsr & FSR_INFEN ? 1 : 0));
	ret +=
	    printf("-> FSR.RDYN: %i\n", (fsr & FSR_RDYN ? 1 : 0));
	ret += printf("-> FSR.WEN: %i\n", (fsr & FSR_WEN ? 1 : 0));
	ret += printf("-> FSR.STP: %i\n", (fsr & FSR_STP ? 1 : 0));
	ret +=
	    printf("-> FSR.ENDEBUG: %i\n",
		    (fsr & FSR_ENDEBUG ? 1 : 0));

end:
	return ret;
}

int _write_infopage(const uint8_t *buf)
{
	int i;
	const uint8_t *infopage = NULL;
	int error_count = 0;

	uint8_t cmd[3 + N_BYTES_FOR_WRITE];
	uint16_t *addr = (uint16_t *) (cmd + 1);

	infopage = buf;

	for (i = 0; i < NRF_PAGE_SIZE; i += N_BYTES_FOR_WRITE) {

		cmd[0] = SPICMD_WREN;
		if (0 > write_then_read(cmd, 1, NULL, 0))
			debug("falha em SPICMD_WREN");

		_wait_for_ready();

		cmd[0] = SPICMD_PROGRAM;
		*addr = htole16(i);
		memcpy(cmd + 3, infopage, N_BYTES_FOR_WRITE);

		if (0 != write_then_read(cmd, 3 + N_BYTES_FOR_WRITE, NULL, 0))
			error_count++;

		infopage += N_BYTES_FOR_WRITE;
		_wait_for_ready();
	}

	return (error_count > 0) ? -error_count : i;
}

int _write_nvm_normal(const uint8_t *buf)
{
	int i;
	const uint8_t *mem = NULL;
	int error_count = 0;

	uint8_t cmd[3 + N_BYTES_FOR_WRITE];
	uint16_t *addr = (uint16_t *) (cmd + 1);

	mem = buf;

	for (i = 0; i < NVM_NORMAL_MEM_SIZE; i += N_BYTES_FOR_WRITE) {

		cmd[0] = SPICMD_WREN;
		if (0 > write_then_read(cmd, 1, NULL, 0))
			debug("falha em SPICMD_WREN");

		_wait_for_ready();

		cmd[0] = SPICMD_PROGRAM;
		*addr = htole16(i + NVM_NORMAL_PAGE0_INI_ADDR);
		memcpy(cmd + 3, mem, N_BYTES_FOR_WRITE);

		if (0 != write_then_read(cmd, 3 + N_BYTES_FOR_WRITE, NULL, 0))
			error_count++;

		mem += N_BYTES_FOR_WRITE;
		_wait_for_ready();
	}

	return (error_count > 0) ? -error_count : i;
}

ssize_t
da_infopage_store(const uint8_t *buf, size_t count)
{
	int ret = 0;
	int size = -1;

	if (0 == _enable_program) {
		debug("falha, enable_program = 0");
		ret = -EINVAL;
		goto end;
	}

	if (NRF_PAGE_SIZE != count) {
		debug("tamanho da infopage(%i) diferente de NRF_PAGE_SIZE(%i)",
		      count, NRF_PAGE_SIZE);
		ret = -EINVAL;
		goto end;
	}

	ret = _enable_infopage_access();
	if (0 != ret)
		goto end;

	_erase_page(0);

	debug("iniciando escrita da memoria");
	ret = _write_infopage(buf);
	if (0 > ret) {
		debug("numero de erros na escrita da infopage: %i", -1 * ret);
	} else {
		debug("bytes escritos na infopage: %i", ret);
		size = ret;
	}
	debug("fim da escrita da memoria");

	ret = _disable_infopage_access();
	if (0 != ret) {
		debug("falha em desabilitar acesso a infopage");
		goto end;
	}

	ret = size;

end:
	return ret;
}

ssize_t
da_infopage_show(uint8_t *buf)
{
	int ret;
	int size;

	if (0 == _enable_program) {
		debug("fail, enable_program = 0");
		ret = -EINVAL;
		goto end;
	}

	debug("begin");

	ret = _enable_infopage_access();
	if (0 != ret)
		goto end;

	size = _read_infopage(buf);
	if (0 > size) {
		debug("falha em ler a infopage, size: %i", size);
	}

	ret = _disable_infopage_access();
	if (0 != ret) {
		debug("falha em desabilitar acesso a infopage");
		goto end;
	}

	ret = size;

end:
	debug("end");
	return ret;
}

ssize_t
da_nvm_normal_show(uint8_t* buf)
{
	int ret;
	int size;

	if (0 == _enable_program) {
		debug("fail, enable_program = 0");
		ret = -EINVAL;
		goto end;
	}

	debug("begin");

	size = _read_nvm_normal(buf);
	if (0 > size) {
		debug("falha em ler a infopage, size: %i", size);
	}

	ret = size;

end:
	debug("end");
	return ret;
}

ssize_t
da_nvm_normal_store(const uint8_t *buf, size_t count)
{
	int ret = 0;
	int size = -1;

	if (0 == _enable_program) {
		debug("falha, enable_program = 0");
		ret = -EINVAL;
		goto end;
	}

	if (NVM_NORMAL_MEM_SIZE != count) {
		debug
		    ("tamanho da imagem(%i) diferente de NVM_NORMAL_MEM_SIZE(%i)",
		     count, NVM_NORMAL_MEM_SIZE);
		ret = -EINVAL;
		goto end;
	}

	_erase_page(NVM_NORMAL_PAGE0);
	_erase_page(NVM_NORMAL_PAGE1);

	debug("iniciando escrita da memoria");
	ret = _write_nvm_normal(buf);
	if (0 > ret) {
		debug("numero de erros na escrita da nvm_normal: %i", -1 * ret);
	} else {
		debug("bytes escritos: %i", ret);
		size = ret;
	}
	debug("fim da escrita da memoria");

	ret = size;

end:
	return ret;
}

void uhet_record_init(void)
{
	printf("iniciando gravacao\n");

	wiring_set_gpio_value(GPIO_PROG, 1);
	mdelay(10);

	wiring_set_gpio_value(GPIO_RESET, 0);
	udelay(5);
	wiring_set_gpio_value(GPIO_RESET, 1);

	mdelay(2);
}

void uhet_record_end(void)
{
	printf("finalizando gravacao\n");

	wiring_set_gpio_value(GPIO_PROG, 0);
	mdelay(1);

	// reset
	wiring_set_gpio_value(GPIO_RESET, 0);
	udelay(1);
	wiring_set_gpio_value(GPIO_RESET, 1);

	mdelay(10);
}

int _toc_toc_tem_alguem_ae(void)
{
	uint8_t out[1] = { 0 };
	uint8_t fsr_after_wren;
	uint8_t fsr_after_wrdis;

	out[0] = SPICMD_WREN;
	write_then_read(out, 1, NULL, 0);

	out[0] = SPICMD_RDSR;
	write_then_read(out, 1, &fsr_after_wren, 1);

	out[0] = SPICMD_WRDIS;
	write_then_read(out, 1, NULL, 0);

	out[0] = SPICMD_RDSR;
	write_then_read(out, 1, &fsr_after_wrdis, 1);

	if ((0 != (fsr_after_wren & FSR_WEN)) &&
	    (0 == (fsr_after_wrdis & FSR_WEN)))
		return 0;

	return -EINVAL;
}

void _erase_all(void)
{
	uint8_t cmd[1];
	int ret;

	cmd[0] = SPICMD_WREN;
	write_then_read(cmd, 1, NULL, 0);
	_wait_for_ready();

	cmd[0] = SPICMD_ERASEALL;
	ret = write_then_read(cmd, 1, NULL, 0);
	if (0 == ret)
		debug("apagando a bagaca toda, ai como eu to bandida");

	_wait_for_ready();
}


uint8_t __enable_wren(void)
{
        uint8_t cmd, fsr;

	cmd = SPICMD_WREN;
	write_then_read(&cmd, 1, NULL, 0);

	cmd = SPICMD_RDSR;
	write_then_read(&cmd, 1, &fsr, 1);

	if((fsr & FSR_WEN ? 1 : 0) == 0)
	{
		printf("Escrita nao pode ser habilitada -> FSR.WEN: %i\n", (fsr & FSR_WEN ? 1 : 0));
		return 0;
	}

	return 1;
}

void _erase_page(unsigned i)
{
	uint8_t cmd[2];
	int ret;

	if(!__enable_wren()) return;

	_wait_for_ready();

	cmd[0] = SPICMD_ERASEPAGE;
	cmd[1] = i;
	ret = write_then_read(cmd, 2, NULL, 0);

	if (0 == ret) printf("apagando a pagina: %i\n", i);

	_wait_for_ready();
}

void _erase_program_pages(void)
{
	unsigned i;


	for (i = 0; i < N_PAGES; i++) {
		_erase_page(i);
	}
}

ssize_t uhet_write(uint8_t *buf, size_t count, unsigned long *off)
{
	unsigned long ret;
	uint8_t cmd[3 + count];
	uint8_t  i =0;
	uint16_t addr = 0;

	if (0 == _enable_program) {
		debug("tentando gravar sendo que enable_pragram = 0");
		return -EINVAL;
	}

	ret = _toc_toc_tem_alguem_ae();
	if (0 != ret) {
		debug("flash nao responde :/");
		return -EINVAL;
	}

	_erase_program_pages(); /* apaga primeiro */

	_wait_for_ready();

	for(i = 0; i < 32; i++)
	{
		cmd[0] = SPICMD_PROGRAM;
		cmd[2] = (uint8_t)(addr & 0x00ff);
		cmd[1] = (uint8_t)((addr & 0xff00) >> 8);


		printf("ADDR: %02X %02X\n",((addr & 0xff00) >> 8), (addr & 0x00ff));
		memcpy(cmd + 3, &buf[addr], 512);

		if(!__enable_wren())
			return -EFAULT;

		printf("b0:%02X    b1:%02X   b2: %02X\n",cmd[0], cmd[2], cmd[1]);
		write_then_read(cmd, 3 + 512, NULL, 0);

		addr = addr + 512; /* Atualizando o endereco */
		_wait_for_ready();
	}

	return count;
}

ssize_t
uhet_read(uint8_t* buf, size_t count, unsigned long *off)
{

	if (0 == _enable_program) {
		debug("falha, enable_program = 0");
		return -EINVAL;
	}

	printf("Quantidade de dados a ser lido: %i   %d\n", count, MAX_FIRMWARE_SIZE);
	// lendo da flash
	{
		uint8_t cmd[3];
		uint16_t *addr = (uint16_t *) (cmd + 1);
		//uint8_t data[count];

		cmd[0] = SPICMD_READ;
		cmd[1] = 0;
		cmd[2] = 0;

		write_then_read(cmd, 3, buf, count);

		debug
		    ("lido addr: 0x%p, pack header: 0x%X 0x%X 0x%X, bytes lidos: %i",
		     addr, cmd[0], cmd[1], cmd[2], count);

		return count;
	}

	return 0;
}

#endif
