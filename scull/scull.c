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

    for (ptr = dev->data; ptr; ptr = next) {
        if (ptr->data) {
            for (i = 0; i < qset; i++) {
                kfree(ptr->data[i]);
            }
            kfree(ptr->data);
            ptr->data = NULL;
        }
        next = ptr->next;
        kfree(ptr);
    }

    dev->size = 0;
    dev->quantum = scull_quantum;
    dev->qset = scull_qset;
    dev->data = NULL;

    return 0;
}

struct scull_qset* scull_follow(struct scull_dev* dev, int n)
{
    struct scull_qset *qs = dev->data;

    if (!qs) {
        qs = dev->data = kmalloc(sizeof(scull_qset), GFP_KERNEL);
        if (!qs)
            return NULL;
        memset(qs, 0, sizeof(struct scull_qset));
    }

    while (n--) {
        if (!qs->next) {
            qs->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
            if (qs->next == NULL)
                return NULL;
            memset(qs->next, 0, sizeof(struct scull_qset));
        }
        qs = qs->next;
        continue;
    }

    return qs;
}

long scull_ioctl(struct file* filp, unsigned int cmd,
        unsigned long arg)
{
    int err = 0, tmp;
    int retval = 0;

    /*
     * check if it is a valid command
     */
    if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
    if (_IOC_NR(cmd) > SCULL_IOC_MAXNR) return -ENOTTY;

    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user *)arg,
                _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ, (void __user *)arg,
                _IOC_SIZE(cmd));

    if (err) return -EFAULT;

    switch(cmd) {
        case SCULL_IOCRESET:
            scull_quantum = SCULL_QUANTUM;
            scull_qset = SCULL_QSET;
            break;

        /* Set: arg points to the value */
        case SCULL_IOCSQUANTUM:
            if (!capable(CAP_SYS_ADMIN)) /* user is root */
                return -EPERM;
            retval = __get_user(scull_quantum, (int __user*) arg);
            break;

        case SCULL_IOCSQSET:
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            retval = __get_user(scull_qset, (int __user*) arg);
            break;

        /* Get: arg is pointer to result */
        case SCULL_IOCGQUANTUM:
            retval = __put_user(scull_quantum, (int __user*) arg);
            break;

        case SCULL_IOCGQSET:
            retval = __put_user(scull_qset, (int __user*) arg);
            break;

        /* Tell: arg is the value */
        case SCULL_IOCTQUANTUM:
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            scull_quantum = arg;
            break;

        case SCULL_IOCTQSET:
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            scull_qset = arg;
            break;

        /* Query: return it*/
        case SCULL_IOCQQUANTUM:
            return scull_quantum;

        case SCULL_IOCQQSET:
            return scull_qset;

        /*eXchange: use arg as pointer*/
        case SCULL_IOCXQUANTUM:
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scull_quantum;
            retval = __get_user(scull_quantum, (int __user*) arg);
            if (retval == 0)
                retval = __put_user(tmp, (int __user*)arg);
            break;
         case SCULL_IOCXQSET:
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scull_qset;
            retval = __get_user(scull_qset, (int __user*) arg);
            if (retval == 0)
                retval = __put_user(tmp, (int __user*)arg);
            break;
        /*sHift: like Tell + Query*/
         case SCULL_IOCHQUANTUM:
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scull_quantum;
            scull_quantum = arg;
            return tmp;
         case SCULL_IOCHQSET:
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scull_qset;
            scull_qset = arg;
            return tmp;
        default:
            return -EINVAL;
    }

    return retval;
}

ssize_t scull_read(struct file* filp, char __user *buf,
        size_t count, loff_t *f_pos)
{
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *ptr;
    int quantum = dev->quantum;
    int qset = dev->qset;
    int item_size = quantum * qset;
    int item, s_pos, q_pos, rest;
    int retval = 0;

    DUMP_STACK();
    if (mutex_lock_interruptible(&dev->mutex))
        return -ERESTARTSYS;
    if (*f_pos >= dev->size)
        goto out;
    if (*f_pos + count > dev->size)
        count = dev->size - *f_pos;

    item = (long) *f_pos / item_size;
    rest = (long) *f_pos % item_size;

    s_pos = rest / quantum;
    q_pos = rest % quantum;

    ptr = scull_follow(dev, item);

    if (ptr == NULL || !ptr->data || !ptr->data[s_pos])
        goto out;
    if (count > quantum - q_pos)
        count = quantum - q_pos;

    if (copy_to_user(buf, ptr->data[s_pos] + q_pos, count)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += count;
    retval = count;
out:
    mutex_unlock(&dev->mutex);
    return retval;
}

ssize_t scull_write(struct file* filp, const char __user *buf,
        size_t count, loff_t *f_pos)
{
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *ptr;
    int quantum = dev->quantum;
    int qset = dev->qset;
    int item_size = quantum * qset;
    int item, s_pos, q_pos, rest;
    int retval = 0;

    DUMP_STACK();
    if (mutex_lock_interruptible(&dev->mutex))
        return -ERESTARTSYS;

    item = (long) *f_pos / item_size;
    rest = (long) *f_pos % item_size;

    s_pos = rest / quantum;
    q_pos = rest % quantum;

    ptr = scull_follow(dev, item);

    if (ptr == NULL)
        goto out;
    if (!ptr->data) {
        ptr->data = kmalloc(qset * sizeof(char*), GFP_KERNEL);
        if (!ptr->data)
            goto out;
        memset(ptr->data, 0, qset * sizeof(char*));
    }

    if (!ptr->data[s_pos]) {
        ptr->data[s_pos] = kmalloc(quantum * sizeof(char),
                GFP_KERNEL);
        if (!ptr->data[s_pos])
            goto out;
        memset(ptr->data[s_pos], 0, quantum * sizeof(char));
    }

    if (count > quantum - q_pos)
        count = quantum - q_pos;

    if (copy_from_user(ptr->data[s_pos] + q_pos, buf, count)) {
        PDEBUG("error happend copy_to_user\n");
        retval = -EFAULT;
        goto out;
    }

    *f_pos += count;
    retval = count;

    if (dev->size < *f_pos)
        dev->size = *f_pos;

out:
    mutex_unlock(&dev->mutex);
    return retval;
}

static int scull_open(struct inode* inode, struct file* filp)
{
    struct scull_dev* dev;
    dev = container_of(inode->i_cdev, struct scull_dev, cdev);

    filp->private_data = dev;

    DUMP_STACK();
    if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        if (mutex_lock_interruptible(&dev->mutex))
            return -ERESTARTSYS;
        scull_trim(dev);
        mutex_unlock(&dev->mutex);
    }

    return 0;
}

int scull_release(struct inode* inode, struct file* filp)
{
    DUMP_STACK();
    return 0;
}

loff_t scull_llseek(struct file* filp, loff_t off, int whence)
{
    struct scull_dev *dev = filp->private_data;
    loff_t newpos;

    switch (whence) {
        case 0: /* SEEK SET */
            newpos = off;
            break;
        case 1: /* SEEK CUR */
            newpos = filp->f_pos + off;
            break;
        case 2: /* SEEK END */
            newpos = dev->size + off;
            break;
        default:
            return -EINVAL;
    }

    if (newpos < 0) return -EINVAL;
    filp->f_pos = newpos;
    return newpos;
}

struct file_operations scull_fops = {
    .owner = THIS_MODULE,
    .open = scull_open,
    .release = scull_release,
    .read = scull_read,
    .write = scull_write,
    .llseek = scull_llseek,
    .unlocked_ioctl = scull_ioctl,
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

    PDEBUG("%s\n", __func__);

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
