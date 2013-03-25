#include <asm/uaccess.h>
#include <asm/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/spi/spi.h>
#include <asm/gpio.h>
#include <linux/net.h>

#define NAME "nrf24le1"

/* spi commands */
#define SPICMD_WREN      0x06
#define SPICMD_WRDIS     0x04
#define SPICMD_RDSR      0x05
#define SPICMD_WRSR      0x01
#define SPICMD_READ      0x03
#define SPICMD_PROGRAM   0x02
#define SPICMD_ERASEPAGE 0x52
#define SPICMD_ERASEALL  0x62
#define SPICMD_RDFPCR    0x89
#define SPICMD_RDISMB    0x85
#define SPICMD_ENDEBUG   0x86

#define FSR_RESERVED0 (1 << 0)
#define FSR_RESERVED1 (1 << 1)
#define FSR_RDISMB    (1 << 2)
#define FSR_INFEN     (1 << 3)
#define FSR_RDYN      (1 << 4)
#define FSR_WEN       (1 << 5)
#define FSR_STP       (1 << 6)
#define FSR_ENDEBUG   (1 << 7)

/* NVM Extended endurance data  pages: 32,33 */
#define NVM_NORMAL_PAGE0           34
#define NVM_NORMAL_PAGE0_INI_ADDR  0x4400
#define NVM_NORMAL_PAGE0_END_ADDR  0x45FF
#define NVM_NORMAL_PAGE1           35
#define NVM_NORMAL_PAGE1_INI_ADDR  0x4600
#define NVM_NORMAL_PAGE1_END_ADDR  0x47FF
#define NVM_NORMAL_NUMBER_OF_PAGES 2
#define NVM_NORMAL_MEM_SIZE        (NVM_NORMAL_NUMBER_OF_PAGES * NRF_PAGE_SIZE)

#define NRF_PAGE_SIZE     (512)
#define N_PAGES           (32)
#define MAX_FIRMWARE_SIZE (NRF_PAGE_SIZE * N_PAGES) /* 16Kb */
#define N_BYTES_FOR_WRITE (16)
#define N_BYTES_FOR_READ  (16)
#define NRF_SPI_SPEED_HZ  (4500 * 1000) /* 4.5Mhz */

#define GPIO_RESET       AT91_PIN_PB29
#define GPIO_PROG        AT91_PIN_PB19

static int uhet_create(void);
static void uhet_destroy(void);
void uhet_record_init(void);
void uhet_record_end(void);

static ssize_t uhet_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t uhet_write(struct file *, const char *, size_t, loff_t *);
static int uhet_open(struct inode *, struct file *);
static int uhet_release(struct inode *, struct file *);

static int uhet_spi_probe(struct spi_device *spi);
static int uhet_spi_remove(struct spi_device *spi);

static ssize_t da_test_show(struct device *, struct device_attribute *, char *);
static ssize_t da_infopage_show(struct device *, struct device_attribute *,
				char *);
static ssize_t da_nvm_normal_show(struct device *, struct device_attribute *,
				  char *);
static ssize_t da_enable_program_show(struct device *,
				      struct device_attribute *, char *);

static ssize_t da_infopage_store(struct device *, struct device_attribute *,
				 const char *, size_t);
static ssize_t da_nvm_normal_store(struct device *, struct device_attribute *,
				   const char *, size_t);
static ssize_t da_enable_program_store(struct device *,
				       struct device_attribute *, const char *,
				       size_t);
static ssize_t da_erase_all_store(struct device *, struct device_attribute *,
				  const char *, size_t);

static void _erase_all(void);
static void _erase_page(unsigned i);
static void _erase_program_pages(void);

static dev_t devno;
static struct class *class = NULL;
static struct cdev cdev;
static struct device *device = NULL;
static struct spi_device *uhet_spi_device = NULL;
static DEFINE_MUTEX(mutex);
static DEVICE_ATTR(infopage, 0600, da_infopage_show, da_infopage_store);
static DEVICE_ATTR(test, 0600, da_test_show, NULL);
static DEVICE_ATTR(enable_program, 0600, da_enable_program_show,
		   da_enable_program_store);
static DEVICE_ATTR(erase_all, 0600, NULL, da_erase_all_store);
static DEVICE_ATTR(nvm_normal, 0600, da_nvm_normal_show, da_nvm_normal_store);

static int _enable_program = 0;

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = uhet_read,
	.write = uhet_write,
	.open = uhet_open,
	.release = uhet_release,
};

static struct spi_driver uhet_spi_driver = {
	.driver = {
		   .name = "nrf24le1",
		   .owner = THIS_MODULE,
		   },
	.probe = uhet_spi_probe,
	.remove = __devexit_p(uhet_spi_remove),
};

#define debug(fmt, args...) \
{ \
	printk(KERN_DEBUG "[" NAME "] %s: " fmt "\n", __func__, ##args); \
}

#define write_then_read(out,n_out,in,n_in) \
({ \
	int __ret = 0; \
\
	__ret = spi_write_then_read(uhet_spi_device,out,n_out,in,n_in); \
	if (0 > __ret){ \
		debug("falha na operacao de write_then_read"); \
	} \
\
	__ret; \
})

static void _wait_for_ready(void)
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

		spi_write_then_read(uhet_spi_device, cmd, 1, fsr, 1);
		udelay(500);

	} while (*fsr & FSR_RDYN);
}

static int _enable_infopage_access(void)
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

static int _read_infopage(char *buf)
{
	int i;
	int ret = 0;

	uint8_t cmd[3];
	uint8_t in[N_BYTES_FOR_READ];

	uint16_t *addr = (uint16_t *) (cmd + 1);
	char *p = buf;

	cmd[0] = SPICMD_READ;
	for (i = 0; i < NRF_PAGE_SIZE; i += N_BYTES_FOR_READ) {

		*addr = htons(i);
		ret = write_then_read(cmd, 3, in, N_BYTES_FOR_READ);
		if (0 != ret)
			return ret;

		memcpy(p, in, N_BYTES_FOR_READ);

		p += N_BYTES_FOR_READ;
	}

	debug("numero de bytes lidos: %i\n", p - buf);

	return p - buf;		// numero de bytes lidos;
}

static int _read_nvm_normal(char *buf)
{
	int i;
	int ret = 0;

	uint8_t cmd[3];
	uint8_t in[N_BYTES_FOR_READ];

	uint16_t *addr = (uint16_t *) (cmd + 1);
	char *p = buf;

	cmd[0] = SPICMD_READ;
	for (i = 0; i < NVM_NORMAL_MEM_SIZE; i += N_BYTES_FOR_READ) {

		*addr = htons(i + NVM_NORMAL_PAGE0_INI_ADDR);
		ret = write_then_read(cmd, 3, in, N_BYTES_FOR_READ);
		if (0 != ret)
			return ret;

		memcpy(p, in, N_BYTES_FOR_READ);

		p += N_BYTES_FOR_READ;
	}

	debug("numero de bytes lidos: %i\n", p - buf);

	return p - buf;		// numero de bytes lidos;
}

static int _disable_infopage_access(void)
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

static ssize_t
da_enable_program_show(struct device *device, struct device_attribute *attr,
		       char *buf)
{
	int ret = mutex_trylock(&mutex);
	if (0 == ret)
		return -ERESTARTSYS;

	ret = sprintf(buf, "%i\n", _enable_program);

	mutex_unlock(&mutex);
	return ret;
}

static ssize_t
da_enable_program_store(struct device *device, struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret = -EINVAL;

	debug("enable program, antes do mutex");

	ret = mutex_trylock(&mutex);
	if (0 == ret) {
		return -ERESTARTSYS;
	}

	debug("enable program, depois do mutex");

	if (1 > count) {
		ret = -EINVAL;
		goto end;
	}

	if (buf[0] != '0' && buf[0] != '1') {
		ret = -EINVAL;
		goto end;
	}

	switch (buf[0]) {

	case '0':
		switch (_enable_program) {
		case 0:
			ret = count;
			goto end;

		case 1:
			_enable_program = 0;
			uhet_record_end();
			ret = count;
			goto end;;
		}

	case '1':
		switch (_enable_program) {
		case 1:
			ret = count;
			goto end;

		case 0:
			_enable_program = 1;
			uhet_record_init();
			_wait_for_ready();
			ret = count;
			goto end;
		}
	}

end:
	mutex_unlock(&mutex);
	return ret;
}

static ssize_t
da_erase_all_store(struct device *device, struct device_attribute *attr,
		   const char *buf, size_t count)
{
	int ret = -EINVAL;

	ret = mutex_trylock(&mutex);
	if (0 == ret)
		return -ERESTARTSYS;

	if (0 == _enable_program) {
		debug("falha, enable_program = 0");
		ret = -EINVAL;
		goto end;
	}

	if (1 > count) {
		ret = -EINVAL;
		goto end;
	}

	if (buf[0] != '1') {
		ret = -EINVAL;
		goto end;
	}

	_erase_all();
	ret = count;

end:
	mutex_unlock(&mutex);
	return ret;
}

static ssize_t
da_test_show(struct device *device, struct device_attribute *attr, char *buf)
{
	uint8_t cmd;
	uint8_t fsr;
	int ret = 0;

	ret = mutex_trylock(&mutex);
	if (0 == ret)
		return -ERESTARTSYS;

	if (0 == _enable_program) {
		debug("falha, enable_program = 0");
		ret = -EINVAL;
		goto end;
	}

	cmd = SPICMD_RDSR;
	write_then_read(&cmd, 1, &fsr, 1);
	ret += sprintf(buf + ret, "* FSR original\n");
	ret +=
	    sprintf(buf + ret, "-> FSR.RDISMB: %i\n",
		    (fsr & FSR_RDISMB ? 1 : 0));
	ret +=
	    sprintf(buf + ret, "-> FSR.INFEN: %i\n", (fsr & FSR_INFEN ? 1 : 0));
	ret +=
	    sprintf(buf + ret, "-> FSR.RDYN: %i\n", (fsr & FSR_RDYN ? 1 : 0));
	ret += sprintf(buf + ret, "-> FSR.WEN: %i\n", (fsr & FSR_WEN ? 1 : 0));
	ret += sprintf(buf + ret, "-> FSR.STP: %i\n", (fsr & FSR_STP ? 1 : 0));
	ret +=
	    sprintf(buf + ret, "-> FSR.ENDEBUG: %i\n",
		    (fsr & FSR_ENDEBUG ? 1 : 0));

	cmd = SPICMD_WREN;
	write_then_read(&cmd, 1, NULL, 0);

	cmd = SPICMD_RDSR;
	write_then_read(&cmd, 1, &fsr, 1);
	ret += sprintf(buf + ret, "* FSR apos WREN, WEN deve ser 1\n");
	ret +=
	    sprintf(buf + ret, "-> FSR.RDISMB: %i\n",
		    (fsr & FSR_RDISMB ? 1 : 0));
	ret +=
	    sprintf(buf + ret, "-> FSR.INFEN: %i\n", (fsr & FSR_INFEN ? 1 : 0));
	ret +=
	    sprintf(buf + ret, "-> FSR.RDYN: %i\n", (fsr & FSR_RDYN ? 1 : 0));
	ret += sprintf(buf + ret, "-> FSR.WEN: %i\n", (fsr & FSR_WEN ? 1 : 0));
	ret += sprintf(buf + ret, "-> FSR.STP: %i\n", (fsr & FSR_STP ? 1 : 0));
	ret +=
	    sprintf(buf + ret, "-> FSR.ENDEBUG: %i\n",
		    (fsr & FSR_ENDEBUG ? 1 : 0));

	cmd = SPICMD_WRDIS;
	write_then_read(&cmd, 1, NULL, 0);

	cmd = SPICMD_RDSR;
	write_then_read(&cmd, 1, &fsr, 1);
	ret += sprintf(buf + ret, "* FSR apos WRDIS, WEN deve ser 0\n");
	ret +=
	    sprintf(buf + ret, "-> FSR.RDISMB: %i\n",
		    (fsr & FSR_RDISMB ? 1 : 0));
	ret +=
	    sprintf(buf + ret, "-> FSR.INFEN: %i\n", (fsr & FSR_INFEN ? 1 : 0));
	ret +=
	    sprintf(buf + ret, "-> FSR.RDYN: %i\n", (fsr & FSR_RDYN ? 1 : 0));
	ret += sprintf(buf + ret, "-> FSR.WEN: %i\n", (fsr & FSR_WEN ? 1 : 0));
	ret += sprintf(buf + ret, "-> FSR.STP: %i\n", (fsr & FSR_STP ? 1 : 0));
	ret +=
	    sprintf(buf + ret, "-> FSR.ENDEBUG: %i\n",
		    (fsr & FSR_ENDEBUG ? 1 : 0));

end:
	mutex_unlock(&mutex);
	return ret;
}

static int _write_infopage(const char *buf)
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
		*addr = htons(i);
		memcpy(cmd + 3, infopage, N_BYTES_FOR_WRITE);

		if (0 != write_then_read(cmd, 3 + N_BYTES_FOR_WRITE, NULL, 0))
			error_count++;

		infopage += N_BYTES_FOR_WRITE;
		_wait_for_ready();
	}

	return (error_count > 0) ? -error_count : i;
}

static int _write_nvm_normal(const char *buf)
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
		*addr = htons(i + NVM_NORMAL_PAGE0_INI_ADDR);
		memcpy(cmd + 3, mem, N_BYTES_FOR_WRITE);

		if (0 != write_then_read(cmd, 3 + N_BYTES_FOR_WRITE, NULL, 0))
			error_count++;

		mem += N_BYTES_FOR_WRITE;
		_wait_for_ready();
	}

	return (error_count > 0) ? -error_count : i;
}

static ssize_t
da_infopage_store(struct device *device, struct device_attribute *attr,
		  const char *buf, size_t count)
{
	int ret = 0;
	int size = -1;

	ret = mutex_trylock(&mutex);
	if (0 == ret) {
		return -ERESTARTSYS;
	}

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
	mutex_unlock(&mutex);
	return ret;
}

static ssize_t
da_infopage_show(struct device *device, struct device_attribute *attr,
		 char *buf)
{
	int ret;
	int size;

	if (0 == mutex_trylock(&mutex))
		return -ERESTARTSYS;

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
	mutex_unlock(&mutex);

	debug("end");
	return ret;
}

static ssize_t
da_nvm_normal_show(struct device *device, struct device_attribute *attr,
		   char *buf)
{
	int ret;
	int size;

	if (0 == mutex_trylock(&mutex))
		return -ERESTARTSYS;

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
	mutex_unlock(&mutex);

	debug("end");
	return ret;
}

static ssize_t
da_nvm_normal_store(struct device *device, struct device_attribute *attr,
		    const char *buf, size_t count)
{
	int ret = 0;
	int size = -1;

	ret = mutex_trylock(&mutex);
	if (0 == ret) {
		return -ERESTARTSYS;
	}

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
	mutex_unlock(&mutex);
	return ret;
}

static int uhet_create(void)
{
	int ret;

	debug("uhet!");

	class = class_create(THIS_MODULE, NAME);
	if (NULL == class) {
		debug("falha em class_create()");
		goto err_class_create;
	}

	ret = alloc_chrdev_region(&devno, 0, 1, NAME);
	if (0 > ret) {
		debug("falha em alloc_chrdev_region()");
		goto err_alloc_chrdev_region;
	}

	cdev_init(&cdev, &fops);
	cdev.owner = THIS_MODULE;

	ret = cdev_add(&cdev, devno, 1);
	if (0 > ret) {
		debug("falha em cdev_add()");
		goto err_cdev_add;
	}

	device = device_create(class, NULL, devno, NULL, "%s", NAME);
	if (NULL == device) {
		debug("falha em device_create()");
		goto err_device_create;
	}

	ret = device_create_file(device, &dev_attr_test);
	ret += device_create_file(device, &dev_attr_erase_all);
	ret += device_create_file(device, &dev_attr_enable_program);
	ret += device_create_file(device, &dev_attr_nvm_normal);
	ret += device_create_file(device, &dev_attr_infopage);
	if (0 != ret) {
		debug("falha em device_create_file()");
		goto err_device_create_file;
	}

	return 0;

err_device_create_file:
	device_destroy(class, devno);

err_device_create:
	cdev_del(&cdev);

err_cdev_add:
	unregister_chrdev_region(devno, 1);

err_alloc_chrdev_region:
	class_destroy(class);

err_class_create:
	return -EINVAL;

}

static void uhet_destroy(void)
{
	device_destroy(class, devno);
	cdev_del(&cdev);
	unregister_chrdev_region(devno, 1);
	class_destroy(class);

	debug("pinando!");
}

void uhet_record_init(void)
{
	printk(KERN_INFO "iniciando gravacao\n");

	at91_set_gpio_value(GPIO_PROG, 1);
	mdelay(10);

	at91_set_gpio_value(GPIO_RESET, 0);
	udelay(5);
	at91_set_gpio_value(GPIO_RESET, 1);

	mdelay(2);
}

void uhet_record_end(void)
{
	printk(KERN_INFO "finalizando gravacao\n");

	at91_set_gpio_value(GPIO_PROG, 0);
	mdelay(1);

	// reset
	at91_set_gpio_value(GPIO_RESET, 0);
	udelay(1);
	at91_set_gpio_value(GPIO_RESET, 1);

	mdelay(10);
}

static int _toc_toc_tem_alguem_ae(void)
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

static void _erase_all(void)
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

static void _erase_page(unsigned i)
{
	uint8_t cmd[2];
	int ret;

	cmd[0] = SPICMD_WREN;
	write_then_read(cmd, 1, NULL, 0);
	_wait_for_ready();

	cmd[0] = SPICMD_ERASEPAGE;
	cmd[1] = i;
	ret = write_then_read(cmd, 2, NULL, 0);
	if (0 == ret) {
		debug("apagando a pagina: %i", i);
	}

	_wait_for_ready();
}

static void _erase_program_pages(void)
{
	unsigned i;
	for (i = 0; i < N_PAGES; i++) {
		_erase_page(i);
	}
}

static ssize_t
uhet_write(struct file *file, const char __user * buf, size_t count,
	   loff_t * off)
{
	uint8_t firmware[N_BYTES_FOR_WRITE];
	unsigned long ret;
	uint8_t cmd[3 + N_BYTES_FOR_WRITE];
	uint16_t *addr = (uint16_t *) (cmd + 1);

	if (0 == _enable_program) {
		debug("tentando gravar sendo que enable_pragram = 0");
		return -EINVAL;
	}

	debug("tamanho do firmware escrito: %i", count);

	if (*off > MAX_FIRMWARE_SIZE) {
		debug("firmware(%i) maior que MAX_FIRMWARE_SIZE(%i)",
		      count, MAX_FIRMWARE_SIZE);
		return -EINVAL;
	}
	// escrevendo apenas o q cabe na flash
	if (*off + count > MAX_FIRMWARE_SIZE)
		count = MAX_FIRMWARE_SIZE - *off;

	// se maior q o q posso tranmitir
	if (count > N_BYTES_FOR_WRITE)
		count = N_BYTES_FOR_WRITE;

	// copiando o firmware
	ret = copy_from_user(firmware, buf, count);
	if (ret != 0) {
		debug("falha dados do usuario");
		return -EINVAL;
	}

	ret = _toc_toc_tem_alguem_ae();
	if (0 != ret) {
		debug("flash nao responde :/");
		return -EINVAL;
	}

	if (0 == *off)
		_erase_program_pages();

	cmd[0] = SPICMD_WREN;
	write_then_read(cmd, 1, NULL, 0);
	if (0 > ret)
		debug("falha em SPICMD_WREN");

	_wait_for_ready();

	cmd[0] = SPICMD_PROGRAM;
	*addr = htons(*off);

	memcpy(cmd + 3, firmware, count);
	ret = write_then_read(cmd, 3 + count, NULL, 0);
	if (0 == ret)
		debug("escrevendo no endereco, 0x%X", *addr);

	_wait_for_ready();

	*off += count;
	return count;
}

static ssize_t
uhet_read(struct file *file, char __user * buf, size_t count, loff_t * off)
{

	if (0 == _enable_program) {
		debug("falha, enable_program = 0");
		return -EINVAL;
	}
	// cabo a parada
	if (*off >= MAX_FIRMWARE_SIZE)
		return 0;

	// se passar do tamanho do firmware
	if (*off + count > MAX_FIRMWARE_SIZE)
		count = MAX_FIRMWARE_SIZE - *off;

	// limitando o tamanho da leitura
	if (count > N_BYTES_FOR_READ)
		count = N_BYTES_FOR_READ;

	// lendo da flash
	{
		uint8_t cmd[3];
		uint16_t *addr = (uint16_t *) (cmd + 1);
		uint8_t data[N_BYTES_FOR_READ];
		int ret;

		cmd[0] = SPICMD_READ;
		*addr = htons(*off);

		write_then_read(cmd, 3, data, count);
		ret = copy_to_user(buf, data, count);
		if (0 != ret)
			debug("problema em copiar dado p/ userspace");

		*off += count;

		debug
		    ("lido addr: 0x%p, pack header: 0x%X 0x%X 0x%X, bytes lidos: %i",
		     addr, cmd[0], cmd[1], cmd[2], count);

		return count;
	}

	return 0;
}

static int uhet_spi_probe(struct spi_device *spi)
{
	int ret = 0;

	uhet_spi_device = spi;
	spi->max_speed_hz = NRF_SPI_SPEED_HZ;

	ret = spi_setup(spi);
	if (0 != ret)
		debug("falha em reconfigurar velocidade da spi");

	debug("probando la vida loka!");

	ret = uhet_create();
	if (0 > ret) {
		debug("falha em criar device :/");
		return -EINVAL;
	}

	/* opa, se conseguimos pegar a spi, ja mando ver nos gpios :) */
	ret += at91_set_GPIO_periph(GPIO_PROG, 0);
	ret += at91_set_GPIO_periph(GPIO_RESET, 1);
	if (0 > ret) {
		debug("falha em setar pinos para GPIO");
		return -EINVAL;
	}

	ret += gpio_request(GPIO_PROG, NAME);
	ret += gpio_request(GPIO_RESET, NAME);
	if (0 > ret) {
		debug("falha em request de GPIOs");
		return -EINVAL;
	}

	ret += gpio_direction_output(GPIO_PROG, 0);
	ret += gpio_direction_output(GPIO_RESET, 1);
	if (0 > ret) {
		debug("falha em setar direcao de GPIOs");
		return -EINVAL;
	}

	debug("GPIOs configurados com sucesso \\o/");
	return 0;
}

static int uhet_spi_remove(struct spi_device *spi)
{
	debug("tchau tchau mundo cruel");
	uhet_destroy();

	gpio_free(GPIO_RESET);
	gpio_free(GPIO_PROG);
	debug("GPIOs liberados!")

	    device_remove_file(device, &dev_attr_erase_all);
	device_remove_file(device, &dev_attr_nvm_normal);
	device_remove_file(device, &dev_attr_infopage);
	device_remove_file(device, &dev_attr_enable_program);
	device_remove_file(device, &dev_attr_test);

	return 0;
}

static int uhet_open(struct inode *inode, struct file *file)
{
	int ret = mutex_trylock(&mutex);
	if (0 == ret) {
		return -ERESTARTSYS;
	}
	debug("open");

	return 0;
}

static int uhet_release(struct inode *inode, struct file *file)
{
	mutex_unlock(&mutex);
	debug("release");

	return 0;
}

static int __init uhet_init(void)
{
	int ret = spi_register_driver(&uhet_spi_driver);
	debug("vida loka \\o/");
	return ret;
}

static void __exit uhet_exit(void)
{
	spi_unregister_driver(&uhet_spi_driver);
	return;
}

module_init(uhet_init);
module_exit(uhet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eder Ruiz Maria <erm@uhet.sh>");
MODULE_VERSION("0.0.1");
MODULE_DESCRIPTION("nrf24le1 flash access driver");
