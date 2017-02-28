obj-m += scull.o
SRC_PATH=/home/awe/beaglebone/ti-linux-kernel
PWD=$(shell pwd)
DEBUG_FLAGS:=

ifeq ($(DEBUG),1)
DEBUG_FLAGS += -g -DSCULL_DEBUG
endif

EXTRA_CFLAGS += $(DEBUG_FLAGS)

all:
	make ARCH=arm CROSS_COMPILE=arm-cortex_a8-linux-gnueabi- -C $(SRC_PATH) M=$(PWD) modules
clean:
	rm -rf *.o *.order *.symvers .tmp_versions *.ko .*.cmd *.mod.*
