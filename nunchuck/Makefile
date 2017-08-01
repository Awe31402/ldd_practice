KERN_DIR=/home/awe/kernel/linux
BEAGLE_ARCH=arm
PWD=$(shell pwd)
obj-m += nunchuck.o
GCC_VER=arm-linux-gnueabihf-

all:
	make ARCH=$(BEAGLE_ARCH) CROSS_COMPILE=$(GCC_VER) M=$(PWD) -C $(KERN_DIR) modules
clean:
	make ARCH=$(BEAGLE_ARCH) CROSS_COMPILE=$(GCC_VER) M=$(PWD) -C $(KERN_DIR) clean
