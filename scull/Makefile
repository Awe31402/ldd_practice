obj-m += scull.o
SRC_PATH=/home/awe/beaglebone/ti-linux-kernel
PWD=$(shell pwd)

all:
	make ARCH=arm CROSS_COMPILE=arm-cortex_a8-linux-gnueabi- -C $(SRC_PATH) M=$(PWD) modules
clean:
	rm -rf *.o *.order *.symvers .tmp_versions *.ko .*.cmd *.mod.*
