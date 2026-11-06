/*
 * File: sms_sensor_driver.c
 * Purpose: Simulated temperature and humidity sensor driver
 * Author: Tran Le Tien Dat - SE205121
 * Date: 2026-11-07
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/math64.h>

#include "sms_sensor.h"
static dev_t sms_dev;
static struct cdev sms_cdev;
static struct class *sms_class;

static struct proc_dir_entry *sms_proc;

static DEFINE_MUTEX(sms_mutex);

static unsigned int interval_ms = SMS_DEFAULT_INTERVAL_MS;

static unsigned long read_count;
static unsigned int last_temp_x100;
static unsigned int last_hum_x100;

static u64 start_time_ms;
static u64 last_read_ms;
static ssize_t sms_read(struct file *file,
                        char __user *user_buf,
                        size_t count,
                        loff_t *ppos)
{
    char buffer[64];
    int len;
    unsigned int temp_x100;
    unsigned int hum_x100;
    u64 now_ms;

    if (*ppos != 0)
        return 0;

    if (count == 0)
        return 0;

    mutex_lock(&sms_mutex);

    now_ms = ktime_to_ms(ktime_get());

    if (read_count > 0 &&
        now_ms - last_read_ms < interval_ms) {
        mutex_unlock(&sms_mutex);
        return -EAGAIN;
    }

    temp_x100 = 1500 + (get_random_u32() % 3001);
    hum_x100 = 3000 + (get_random_u32() % 6001);

    len = scnprintf(buffer, sizeof(buffer),
                    "%llu,%u.%02u,%u.%02u\n",
                    (unsigned long long)now_ms,
                    temp_x100 / 100, temp_x100 % 100,
                    hum_x100 / 100, hum_x100 % 100);

    if (count < len) {
        mutex_unlock(&sms_mutex);
        return -EINVAL;
    }

    if (copy_to_user(user_buf, buffer, len)) {
        mutex_unlock(&sms_mutex);
        return -EFAULT;
    }

    read_count++;
    last_temp_x100 = temp_x100;
    last_hum_x100 = hum_x100;
    last_read_ms = now_ms;

    *ppos += len;

    mutex_unlock(&sms_mutex);

    return len;
}
static long sms_ioctl(struct file *file,
                      unsigned int cmd,
                      unsigned long arg)
{
    unsigned int new_interval;

    if (cmd != SMS_SET_INTERVAL)
        return -ENOTTY;

    if (copy_from_user(&new_interval,
                       (unsigned int __user *)arg,
                       sizeof(new_interval)))
        return -EFAULT;

    if (new_interval == 0 || new_interval > 60000)
        return -EINVAL;

    mutex_lock(&sms_mutex);

    interval_ms = new_interval;

    mutex_unlock(&sms_mutex);

    return 0;
}
static int sms_proc_show(struct seq_file *m, void *v)
{
    u64 uptime_seconds;

    mutex_lock(&sms_mutex);

    uptime_seconds =
        div_u64(ktime_to_ms(ktime_get()) - start_time_ms, 1000);

    seq_printf(m, "read_count: %lu\n", read_count);

    seq_printf(m, "last_temperature: %u.%02u\n",
               last_temp_x100 / 100,
               last_temp_x100 % 100);

    seq_printf(m, "last_humidity: %u.%02u\n",
               last_hum_x100 / 100,
               last_hum_x100 % 100);

    seq_printf(m, "uptime_seconds: %llu\n",
               (unsigned long long)uptime_seconds);

    mutex_unlock(&sms_mutex);

    return 0;
}

static int sms_proc_open(struct inode *inode,
                         struct file *file)
{
    return single_open(file, sms_proc_show, NULL);
}

static const struct proc_ops sms_proc_ops = {
    .proc_open    = sms_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};
static const struct file_operations sms_fops = {
    .owner          = THIS_MODULE,
    .read           = sms_read,
    .unlocked_ioctl = sms_ioctl,
    .llseek         = default_llseek,
};

static int __init sms_init(void)
{
    int ret;

    start_time_ms = ktime_to_ms(ktime_get());

    ret = alloc_chrdev_region(&sms_dev, 0, 1, SMS_DEVICE_NAME);
    if (ret < 0)
        return ret;

    cdev_init(&sms_cdev, &sms_fops);
    sms_cdev.owner = THIS_MODULE;

    ret = cdev_add(&sms_cdev, sms_dev, 1);
    if (ret)
        goto err_unregister;

    sms_class = class_create(THIS_MODULE, SMS_DEVICE_NAME);
    if (IS_ERR(sms_class)) {
        ret = PTR_ERR(sms_class);
        goto err_cdev;
    }

    if (IS_ERR(device_create(sms_class, NULL, sms_dev,
                             NULL, SMS_DEVICE_NAME))) {
        ret = -ENODEV;
        goto err_class;
    }

    sms_proc = proc_create("sms_stats", 0444, NULL, &sms_proc_ops);
    if (!sms_proc) {
        ret = -ENOMEM;
        goto err_device;
    }

    pr_info("sms_sensor: driver loaded\n");
    return 0;

err_device:
    device_destroy(sms_class, sms_dev);
err_class:
    class_destroy(sms_class);
err_cdev:
    cdev_del(&sms_cdev);
err_unregister:
    unregister_chrdev_region(sms_dev, 1);
    return ret;
}

static void __exit sms_exit(void)
{
    proc_remove(sms_proc);
    device_destroy(sms_class, sms_dev);
    class_destroy(sms_class);
    cdev_del(&sms_cdev);
    unregister_chrdev_region(sms_dev, 1);

    pr_info("sms_sensor: driver unloaded\n");
}

module_init(sms_init);
module_exit(sms_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tran Le Tien Dat - SE205121");
MODULE_DESCRIPTION("SMS temperature and humidity sensor driver");
