#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/input.h>

#define DRIVER_NAME "nunchuck"
#define NUNCHUCK_SLEEP_MS 10000
#define DELTA (s8) 20

struct task_struct *nunchuck_tsk;

struct nunchuck_dev
{
    struct i2c_client *i2c_client;
    struct input_dev *input_dev;
};

static int nunchuck_thread(void* data)
{
    struct nunchuck_dev* nunchuck =
        (struct nunchuck_dev*) data;

    struct input_dev* input = nunchuck->input_dev;

    s8 rel_data = DELTA;

    while (!kthread_should_stop()) {
        printk(KERN_INFO "[%s] FUNC: %s, LINE: %d: Thread running\n",
            DRIVER_NAME, __func__, __LINE__);

        input_report_key(input, EV_KEY, BTN_LEFT, 1);
        input_report_rel(input, EV_REL, REL_X, rel_data);
        input_report_rel(input, EV_REL, REL_Y, rel_data);
        input_sync(input);
        msleep(NUNCHUCK_SLEEP_MS);
    }

    return 0;
}

static int nunchuck_probe(struct i2c_client* client,
        const struct i2c_device_id* id)
{
    struct nunchuck_dev *nunchuck = NULL;
    struct input_dev *input = NULL;
    int result;

    dump_stack();
    printk(KERN_INFO "[%s] FUNC: %s, LINE: %d: Hello nunchuck!\n",
            DRIVER_NAME, __func__, __LINE__);

    nunchuck = devm_kzalloc(&client->dev, sizeof(struct nunchuck_dev),
            GFP_KERNEL);

    nunchuck->i2c_client = client;
    i2c_set_clientdata(client, nunchuck);

    if (!nunchuck) {
        printk(KERN_INFO "[%s] FUNC: %s, LINE: %d:"
                " malloc struct nunchuck_dev failed\n",
            DRIVER_NAME, __func__, __LINE__);
        return -ENOMEM;
    }

    input = input_allocate_device();
    if (!input) {
        printk(KERN_INFO "[%s] FUNC: %s, LINE: %d:"
                " allocate input device failed\n",
            DRIVER_NAME, __func__, __LINE__);
        return -ENOMEM;
    }

    input->name = DRIVER_NAME;
    input->id.bustype = BUS_I2C;
    input->id.vendor = 0x1234;
    input->id.product = 0xABCD;
    input->dev.parent = &client->dev;
    input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
    input->relbit[BIT_WORD(REL_X)] =
        BIT_MASK(REL_X) | BIT_MASK(REL_Y);
    input->keybit[BIT_WORD(BTN_LEFT)] =
        BIT_MASK(BTN_LEFT) | BIT_MASK(BTN_RIGHT) | BIT_MASK(BTN_MIDDLE);

    __set_bit(INPUT_PROP_POINTER, input->propbit);
    __set_bit(INPUT_PROP_POINTING_STICK, input->propbit);
    nunchuck->input_dev = input;

    result = input_register_device(nunchuck->input_dev);

    if (result < 0)
        goto register_err;

    nunchuck_tsk = kthread_run(nunchuck_thread,
            nunchuck, "nunchuck_thread");

    return 0;

register_err:
    input_free_device(nunchuck->input_dev);
    return -ENOMEM;
}

static int nunchuck_remove(struct i2c_client* client)
{
    struct nunchuck_dev *nunchuck = i2c_get_clientdata(client);
    struct input_dev* input = nunchuck->input_dev;
    dump_stack();
    printk(KERN_INFO "[%s] FUNC: %s, LINE: %d: Goodbye nunchuck!\n",
            DRIVER_NAME, __func__, __LINE__);

    kthread_stop(nunchuck_tsk);
    input_unregister_device(input);
    input_free_device(input);

    return 0;
}

static const struct of_device_id nunchuck_dt_ids[] = {
    {.compatible = "nintendo,nunchuck"},
    {},
};
MODULE_DEVICE_TABLE(of, nunchuck_dt_ids);

static const struct i2c_device_id nunchuk_id[] = {
    {"nunchuck", 0},
    {},
};
MODULE_DEVICE_TABLE(i2c, nunchuk_id);

static struct i2c_driver nunckuck_driver = {
    .probe = nunchuck_probe,
    .remove = nunchuck_remove,
    .id_table = nunchuk_id,
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(nunchuck_dt_ids),
    },
};
module_i2c_driver(nunckuck_driver);

MODULE_LICENSE("Dual BSD/GPL");
