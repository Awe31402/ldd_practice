#ifndef SCULL_H
#define SCULL_H

#ifndef SCULL_MAJOR
#define SCULL_MAJOR 0
#endif

#ifndef SCULL_NR_DEVS
#define SCULL_NR_DEVS 4
#endif

#ifndef SCULL_QUANTUM
#define SCULL_QUANTUM 4000
#endif

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
#endif

#endif
