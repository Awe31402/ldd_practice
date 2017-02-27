#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>

#include "scull.h"

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;
struct class* scull_class = NULL;

#define SCULL_NAME "scull"

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);

struct scull_dev* scull_devices;

static int scull_trim(struct scull_dev* dev)
{
    struct scull_qset *next, *ptr;

    int qset = dev->qset;
    int i;

    for(ptr = dev->data; ptr; ptr = next) {
        printk(KERN_INFO "scull_trim: trimming scull_devices %p",
                ptr);
        if (ptr->data) {
            for (i = 0; i < qset; i++) {
                printk(KERN_INFO
                        "scull_trim: freeing ptr->data[%d]", i);
                kfree(ptr->data[i]);
            }
            printk(KERN_INFO
                        "scull_trim: freeing ptr->data");
            kfree(ptr->data);
            ptr->data = NULL;
        }
        next = ptr->next;
        printk(KERN_INFO
                        "scull_trim: freeing ptr");
        kfree(ptr);
    }

    dev->size = 0;
    dev->quantum = scull_quantum;
    dev->qset = scull_qset;
    dev->data = NULL;

    return 0;
}

struct file_operations scull_fops = {
    .owner = THIS_MODULE,
};

inline void scull_cleanup(void)
{
    int i;
    dev_t devno = MKDEV(scull_major, scull_minor);

    if (scull_devices) {
        for (i = 0; i < scull_nr_devs; i++) {
            scull_trim(scull_devices + i);
            cdev_del(&scull_devices[i].cdev);
            device_destroy(scull_class, MKDEV(scull_major, i));
        }
        kfree(scull_devices);
    }

    if (scull_class)
        class_destroy(scull_class);
    unregister_chrdev_region(devno, scull_nr_devs);
}

static void __exit scull_exit(void)
{
    scull_cleanup();
}

static void scull_setup_cdev(struct scull_dev* dev, int index)
{
    int err, devno = MKDEV(scull_major,scull_minor + index);
    cdev_init(&dev->cdev, &scull_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &scull_fops;
    err = cdev_add(&dev->cdev, devno, 1);

    device_create(scull_class,
            NULL,
            MKDEV(scull_major, index),
            NULL,
            "scull%d", index);

    if (err)
        printk(KERN_NOTICE "Error %d: adding scull %d\n", err,
                index);
}

static int __init scull_init(void)
{
    int result, i;
    dev_t dev = 0;

    if (scull_major) {
        dev = MKDEV(scull_major, scull_minor);
        result = register_chrdev_region(dev, scull_nr_devs,
                SCULL_NAME);
    } else {
        result = alloc_chrdev_region(&dev, scull_major, scull_nr_devs,
                SCULL_NAME);
        scull_major = MAJOR(dev);
    }

    if (result < 0) {
        printk(KERN_WARNING "scull: can't get major %d\n",
                scull_major);
        return result;
    }

    scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev),
            GFP_KERNEL);
    if (!scull_devices) {
        printk(KERN_INFO "scull: kammloc faild \n");
        result = -ENOMEM;
        goto fail;
    }

    memset(scull_devices, 0,
            scull_nr_devs * sizeof(struct scull_dev));

    scull_class = class_create(THIS_MODULE, SCULL_NAME);

    printk(KERN_INFO "check class\n");
    if (IS_ERR(scull_class)) {
        result = -EFAULT;
        printk(KERN_WARNING "%s: error class_create\n", SCULL_NAME);
        goto fail;
    }

    for (i = 0; i < scull_nr_devs; i++) {
        scull_devices[i].quantum = scull_quantum;
        scull_devices[i].qset = scull_qset;
        mutex_init(&scull_devices[i].mutex);
        scull_setup_cdev(&scull_devices[i], i);
    }

    return 0;
fail:
    scull_cleanup();
    return result;
}

module_init(scull_init);
module_exit(scull_exit);
MODULE_LICENSE("Dual BSD/GPL");
