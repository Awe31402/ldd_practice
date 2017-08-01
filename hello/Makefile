SRC_PATH=/home/awe/beaglebone/ti-linux-kernel
obj-m += hello.o
PWD=$(shell pwd)

all:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C $(SRC_PATH) M=$(PWD) modules
clean:
	rm -rf *.o *.order *.symvers .tmp_versions *.ko .*.cmd *.mod.*
