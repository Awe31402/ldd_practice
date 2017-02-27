#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

static int __init hello_init(void)
{
    printk("hello module init\n");
    return 0;
}

static void __exit hello_exit(void)
{
    printk("hello module exit\n");
}

module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("Dual BSD/GPL");
