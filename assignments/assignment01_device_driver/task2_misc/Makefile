
obj-m += counter_driver.o

KDIR := $(HOME)/embedded_lab1/kernel/linux-5.15

ARCH := arm
CROSS_COMPILE := arm-linux-gnueabihf-

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) \
		ARCH=$(ARCH) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		EXTRA_CFLAGS="-march=armv7-a" modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) \
		ARCH=$(ARCH) \
		CROSS_COMPILE=$(CROSS_COMPILE) clean
