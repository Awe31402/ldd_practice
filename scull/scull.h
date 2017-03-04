#ifndef SCULL_H
#define SCULL_H

#ifndef SCULL_MAJOR
#define SCULL_MAJOR 0
#endif /* SCULL_MAJOR*/

#ifndef SCULL_NR_DEVS
#define SCULL_NR_DEVS 4
#endif /* SCULL_NR_DEVS */

#ifndef SCULL_QUANTUM
#define SCULL_QUANTUM 4000
#endif /* SCULL_QUANTUM */

#ifndef SCULL_QSET
#define SCULL_QSET 1000
#endif

struct scull_qset {
    void** data;
    struct scull_qset* next;
};

struct scull_dev {
    struct scull_qset* data;
    int quantum;
    int qset;
    unsigned long size;
    unsigned int access_key;
    struct mutex mutex;
    struct cdev cdev;
};

extern int scull_major;
extern int scull_nr_devs;
extern int scull_qset;
extern int scull_quantum;

#ifdef SCULL_DEBUG
    #define PDEBUG(fmt, args...) printk(KERN_INFO "scull: " fmt, ## args)
    #define DUMP_STACK() dump_stack()
#else
    #define PDEBUG(fmt, args...) ;
    #define DUMP_STACK() ;
#endif /* SCULL_DEBUG */

#include <linux/ioctl.h>

#define TYPE(minor) (((minor) >> 4) & 0xf)
#define NUM(minor)  ((minor) & 0xf)

/*
 * IOCTL definitions
 */
#define SCULL_IOC_MAGIC 'k'

#define SCULL_IOCRESET _IO(SCULL_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr
 * T means "Tell" directly with the argument value
 * G means "Get" reply by setting through a pointer
 * Q means "Query" response is on the return value
 * X means "eXchange" switch G and S atomically
 * H means "sHift" switch T and Q atomically
 */
#define SCULL_IOCSQUANTUM _IOW(SCULL_IOC_MAGIC, 1, int)
#define SCULL_IOCSQSET    _IOW(SCULL_IOC_MAGIC, 2, int)
#define SCULL_IOCTQUANTUM _IO(SCULL_IOC_MAGIC, 3)
#define SCULL_IOCTQSET    _IO(SCULL_IOC_MAGIC, 4)
#define SCULL_IOCGQUANTUM _IOR(SCULL_IOC_MAGIC, 5, int)
#define SCULL_IOCGQSET    _IOR(SCULL_IOC_MAGIC, 6, int)
#define SCULL_IOCQQUANTUM _IO(SCULL_IOC_MAGIC, 7)
#define SCULL_IOCQQSET    _IO(SCULL_IOC_MAGIC, 8)
#define SCULL_IOCXQUANTUM _IOWR(SCULL_IOC_MAGIC, 9, int)
#define SCULL_IOCXQSET    _IOWR(SCULL_IOC_MAGIC, 10, int)
#define SCULL_IOCHQUANTUM _IO(SCULL_IOC_MAGIC, 11)
#define SCULL_IOCHQSET _IO(SCULL_IOC_MAGIC, 12)

#define SCULL_IOC_MAXNR 12
#endif /*SCULL_H*/
