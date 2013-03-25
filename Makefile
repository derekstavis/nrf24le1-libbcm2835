PWD   := $(shell pwd)
obj-m := nrf24le1.o


default: validate
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean: validate
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

validate:
	test -n $(KERNELDIR)
	test -d $(KERNELDIR)
