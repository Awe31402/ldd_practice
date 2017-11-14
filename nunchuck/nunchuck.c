#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>

#define DRIVER_NAME "nunchuck"
#define THREAD_SLEEP_MS 50
#define JOYSTICK_SCALE_FACTOR 3
#define JOYSTICK_IGNORE_THREASHOLD 5
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

struct nunchuck_params {
    int joystick_scale_factor;
    unsigned int thread_sleep_ms;
    int joystick_ignore_threshold;
    struct mutex lock;
};

struct nunchuck_params params = {
    .joystick_scale_factor = JOYSTICK_SCALE_FACTOR,
    .thread_sleep_ms = THREAD_SLEEP_MS,
    .joystick_ignore_threshold = JOYSTICK_IGNORE_THREASHOLD,
};

struct kobject attr_kobj = {
    .name = "control",
};

static ssize_t scale_factor_show(struct device* dev,
        struct device_attribute *attr, char* buffer)
{
    int scf;
    if (mutex_lock_interruptible(&params.lock))
        return -EIO;
    scf = params.joystick_scale_factor;
    mutex_unlock(&params.lock);
    return sprintf(buffer, "%d\n", scf);
}

static ssize_t scale_factor_store(struct device* dev,
        struct device_attribute* attr, const char* buffer, size_t count)
{
    int scf = -1;
    if (mutex_lock_interruptible(&params.lock))
        return -EIO;
    sscanf(buffer, "%d", &scf);

    if (scf == -1) {
        mutex_unlock(&params.lock);
        return -EINVAL;
    }

    params.joystick_scale_factor = scf;

    mutex_unlock(&params.lock);
    return count;
}

static DEVICE_ATTR(scale_factor, S_IRUGO | S_IWUSR , scale_factor_show, scale_factor_store);

static ssize_t sleep_ms_show(struct device* dev,
        struct device_attribute* attr, char* buffer)
{
    unsigned int sms;
    if (mutex_lock_interruptible(&params.lock))
        return -EIO;
    sms = params.thread_sleep_ms;
    mutex_unlock(&params.lock);
    return sprintf(buffer, "%u\n", sms);
}

static ssize_t sleep_ms_store(struct device* dev,
        struct device_attribute* attr, const char* buffer, size_t count)
{
    unsigned int sms = 0;
    if (mutex_lock_interruptible(&params.lock))
        return -EIO;
    sscanf(buffer, "%u", &sms);

    if (sms <= 0) {
        mutex_unlock(&params.lock);
        return -EINVAL;
    }

    params.thread_sleep_ms = sms;

    mutex_unlock(&params.lock);
    return count;
}

static DEVICE_ATTR(sleep_ms, S_IRUGO | S_IWUSR, sleep_ms_show, sleep_ms_store);

static ssize_t threshold_show(struct device* dev,
        struct device_attribute *attr, char* buffer)
{
    int sts;
    if (mutex_lock_interruptible(&params.lock))
        return -EIO;
    sts = params.joystick_ignore_threshold;
    mutex_unlock(&params.lock);
    return sprintf(buffer, "%u\n", sts);
}

static ssize_t threshold_store(struct device* dev,
        struct device_attribute* attr, const char* buffer, size_t count)
{
    int sts = -1;
    if (mutex_lock_interruptible(&params.lock))
        return -EIO;
    sscanf(buffer, "%d", &sts);

    if (sts < 0) {
        mutex_unlock(&params.lock);
        return -EINVAL;
    }

    params.joystick_ignore_threshold = sts;

    mutex_unlock(&params.lock);
    return count;
}

static DEVICE_ATTR(threshold, S_IRUGO | S_IWUSR, threshold_show, threshold_store);

const struct attribute* nunparam_attrs[] = {
    &dev_attr_threshold.attr,
    &dev_attr_scale_factor.attr,
    &dev_attr_sleep_ms.attr,
    NULL,
};

int parse_nunchuck_signal(struct nunchuck_signal* signal,
        char* buf, int count, int scale_factor, int threshold)
{
    if (count < 6)
        return -1;

    signal->jx = (int) (buf[0] - (char) 128) / scale_factor;
    signal->jy = (int) ((char) 128 - buf[1]) / scale_factor;
    signal->bz = (~buf[5]) & 0x01;
    signal->bc = ((~buf[5]) >> 1) & 0x01;
    signal->ax = (buf[5] >> 2 & 0x3) | (buf[2] << 2);
    signal->ay = (buf[5] >> 4 & 0x3) | (buf[3] << 2);
    signal->az = (buf[5] >> 6 & 0x3) | (buf[4] << 2);

    if (ABS(signal->jx) < threshold)
        signal->jx = 0;
    if (ABS(signal->jy) < threshold)
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

    int threshold;
    unsigned int sleep_ms;
    int scale_factor;

    while (!kthread_should_stop()) {
        if (nunchuck_read_registers(client, buf, sizeof(buf)) < 0)
            goto next_loop;
        if (mutex_lock_interruptible(&params.lock))
            goto stop;

        threshold = params.joystick_ignore_threshold;
        sleep_ms = params.thread_sleep_ms;
        scale_factor = params.joystick_scale_factor;

        mutex_unlock(&params.lock);
        if (!parse_nunchuck_signal(&sig, buf, sizeof(buf),
                    scale_factor, threshold)) {
            input_report_key(input, BTN_LEFT, sig.bc);
            input_report_key(input, BTN_RIGHT, sig.bz);
            input_report_rel(input, REL_X, sig.jx);
            input_report_rel(input, REL_Y, sig.jy);
            input_sync(input);
        }
next_loop:
        msleep(params.thread_sleep_ms);
    }
stop:
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

    mutex_init(&params.lock);
    result = sysfs_create_files(&client->dev.kobj, nunparam_attrs);
    if (result)
        goto create_file_error;

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
    sysfs_remove_files(&client->dev.kobj, nunparam_attrs);
create_file_error:
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
    sysfs_remove_files(&client->dev.kobj, nunparam_attrs);
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
