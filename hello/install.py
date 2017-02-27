import os
import time

module_name = 'hello'

if __name__ == '__main__'
    print("loading %s module" % module_name)
    os.system('insmod hello.ko')
    os.system('dmesg | grep ' + module_name)
    time.sleep(5)
    print("removing %s module" % module_name)
    os.system('rmmod ' + module_name)
    os.system('dmesg | grep ' + module_name)
