DEBUG = n

ifeq ($(DEBUG),y)
DEBFLAGS = -O -g -DSCULL_DEBUG
else
DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)

ifneq ($(KERNELRELEASE),)

obj-m := mytty0tty.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions modules.order Module.symvers

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

install_udev:
	install -m 644 50-mytty0tty.rules /etc/udev/rules.d/50-mytty0tty.rules
	udevadm control --reload-rules
	udevadm trigger

install: modules_install install_udev
	depmod -a

endif
