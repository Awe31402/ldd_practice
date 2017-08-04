#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/input.h>

#define DRIVER_NAME "nunchuck"
#define THREAD_SLEEP_MS 50
#define JOYSTICK_SCALE_FACTOR 3
#define JOYSTICK_IGNORE_THREASHOLD 5

int joystick_scale_factor = JOYSTICK_SCALE_FACTOR;
unsigned int thread_sleep_ms = THREAD_SLEEP_MS;
int joystick_ignore_threshold = JOYSTICK_IGNORE_THREASHOLD;

#define usleep(micro_sec) usleep_range(micro_sec, micro_sec + 500)

struct task_struct *nunchuck_tsk;

struct nunchuck_dev {
    struct i2c_client *i2c_client;
    struct input_dev *input_dev;
};

struct nunchuck_signal {
    int jx;
    int jy;
    int bz;
    int bc;
    int ax;
    int ay;
    int az;
};

#define ABS(i) (((i) < 0)? (-1 * (i)) : (i))

int parse_nunchuck_signal(struct nunchuck_signal* signal,
        char* buf, int count)
{
    if (count < 6)
        return -1;

    signal->jx = (int) (buf[0] - (char) 128) / joystick_scale_factor;
    signal->jy = (int) ((char) 128 - buf[1]) / joystick_scale_factor;
    signal->bz = (~buf[5]) & 0x01;
    signal->bc = ((~buf[5]) >> 1) & 0x01;
    signal->ax = (buf[5] >> 2 & 0x3) | (buf[2] << 2);
    signal->ay = (buf[5] >> 4 & 0x3) | (buf[3] << 2);
    signal->az = (buf[5] >> 6 & 0x3) | (buf[4] << 2);

    if (ABS(signal->jx) < joystick_ignore_threshold)
        signal->jx = 0;
    if (ABS(signal->jy) < joystick_ignore_threshold)
        signal->jy = 0;

    /*printk(KERN_INFO "[%s] jx: %d jy: %d bz: %d bc: %d ax: %d ay: %d az: %d\n",
            DRIVER_NAME, signal->jx, signal->jy, signal->bz, signal->bc,
            signal->ax, signal->ay, signal->az);*/
    return 0;
}

int nunchuck_write_registers(struct i2c_client* client,
        char* buf, int count)
{
    int status;
    status = i2c_master_send(client, buf, count);
    if (status < 0) {
        printk(KERN_INFO "Error writing nunchuck\n");
        return status;
    }

    usleep(1000);
    return status;
}

int nunchuck_read_registers(struct i2c_client* client,
        char* buf, int count)
{
    int status;
    char read[] = {0x00};

    msleep(10);
    nunchuck_write_registers(client, read, sizeof(read));
    msleep(9);

    status = i2c_master_recv(client, buf, count);
    if (status < 0) {
        printk(KERN_INFO "Error writing nunchuck\n");
        return status;
    }

    return status;
}

static int nunchuck_thread(void* data)
{
    struct nunchuck_dev* nunchuck =
        (struct nunchuck_dev*) data;

    struct input_dev* input = nunchuck->input_dev;
    struct i2c_client* client = nunchuck->i2c_client;

    char buf[6];
    struct nunchuck_signal sig;

    while (!kthread_should_stop()) {
        if (nunchuck_read_registers(client, buf, sizeof(buf)) < 0)
            goto next_loop;
        if (!parse_nunchuck_signal(&sig, buf, sizeof(buf))) {
            input_report_key(input, BTN_LEFT, sig.bc);
            input_report_key(input, BTN_RIGHT, sig.bz);
            input_report_rel(input, REL_X, sig.jx);
            input_report_rel(input, REL_Y, sig.jy);
            input_sync(input);
        }
next_loop:
        msleep(thread_sleep_ms);
    }

    return 0;
}

static int nunchuck_probe(struct i2c_client* client,
        const struct i2c_device_id* id)
{
    struct nunchuck_dev *nunchuck = NULL;
    struct input_dev *input = NULL;
    int result;
    char init1[] = {0xf0, 0x55};
    char init2[] = {0xfb, 0x00};

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

    result = nunchuck_write_registers(client, init1, sizeof(init1));
    if (result < 0)
        goto write_error;
    result = nunchuck_write_registers(client, init2, sizeof(init2));
    if (result < 0)
        goto write_error;

    nunchuck_tsk = kthread_run(nunchuck_thread,
            nunchuck, "nunchuck_thread");

    return 0;

write_error:
    input_unregister_device(nunchuck->input_dev);
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
