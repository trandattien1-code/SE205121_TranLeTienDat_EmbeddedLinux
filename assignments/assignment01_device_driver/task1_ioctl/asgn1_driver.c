
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/errno.h>

#include "asgn1_ioctl.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tran Le Tien Dat - SE205121");
MODULE_DESCRIPTION("ASSIGNMENT-01: Character Driver with ioctl");
MODULE_VERSION("1.0");

#define DEVICE_NAME  "asgn1"
#define CLASS_NAME   "asgn1_class"
#define BUFFER_SIZE  1024
#define DRIVER_MAJOR 241

static int major_number = DRIVER_MAJOR;

static char device_buffer[BUFFER_SIZE];
static int buffer_len;

static int open_count;
static int write_count;
static int read_count;
static int last_write_size;

/* 0 = Kernel mode, 1 = Echo mode */
static int current_mode = 1;

static DEFINE_MUTEX(asgn1_mutex);

static struct class *asgn1_class;
static struct device *asgn1_device;
static struct proc_dir_entry *proc_entry;
static bool sysfs_group_created;

/* ===================================================
 * PROCFS
 * =================================================== */

/*
 * Display driver statistics through /proc/asgn1_info.
 * m: seq_file output destination.
 * v: iterator data (unused).
 * Returns 0 on success.
 */
static int asgn1_proc_show(struct seq_file *m, void *v)
{
    mutex_lock(&asgn1_mutex);

    seq_printf(m, "ASSIGNMENT-01 Driver Statistics\n");
    seq_printf(m, "===============================\n");
    seq_printf(m, "Driver name     : %s\n", DEVICE_NAME);
    seq_printf(m, "Major number    : %d\n", major_number);
    seq_printf(m, "Buffer size     : %d\n", BUFFER_SIZE);
    seq_printf(m, "Buffer length   : %d\n", buffer_len);
    seq_printf(m, "Open count      : %d\n", open_count);
    seq_printf(m, "Write count     : %d\n", write_count);
    seq_printf(m, "Read count      : %d\n", read_count);
    seq_printf(m, "Last write size : %d\n", last_write_size);
    seq_printf(m, "Current mode    : %d\n", current_mode);

    if (buffer_len > 0)
        seq_printf(m, "Last data       : [%s]\n",
                   device_buffer);

    mutex_unlock(&asgn1_mutex);

    return 0;
}

/*
 * Open the procfs statistics file.
 * Returns the result of single_open().
 */
static int asgn1_proc_open(struct inode *inode,
                          struct file *file)
{
    return single_open(file, asgn1_proc_show, NULL);
}

static const struct proc_ops asgn1_proc_fops = {
    .proc_open    = asgn1_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* ===================================================
 * SYSFS
 * =================================================== */

/* Return the current buffer length. */
static ssize_t buffer_len_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    int value;

    mutex_lock(&asgn1_mutex);
    value = buffer_len;
    mutex_unlock(&asgn1_mutex);

    return sysfs_emit(buf, "%d\n", value);
}

/* Return the total number of device opens. */
static ssize_t open_count_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    int value;

    mutex_lock(&asgn1_mutex);
    value = open_count;
    mutex_unlock(&asgn1_mutex);

    return sysfs_emit(buf, "%d\n", value);
}

/* Return the most recently stored device data. */
static ssize_t last_data_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
    ssize_t ret;

    mutex_lock(&asgn1_mutex);

    if (buffer_len > 0)
        ret = sysfs_emit(buf, "%s\n", device_buffer);
    else
        ret = sysfs_emit(buf, "(empty)\n");

    mutex_unlock(&asgn1_mutex);

    return ret;
}

static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RO(last_data);

static struct attribute *asgn1_attrs[] = {
    &dev_attr_buffer_len.attr,
    &dev_attr_open_count.attr,
    &dev_attr_last_data.attr,
    NULL,
};

static const struct attribute_group asgn1_attr_group = {
    .attrs = asgn1_attrs,
};

/* ===================================================
 * FILE OPERATIONS
 * =================================================== */

/*
 * Open the character device.
 * Increments open_count under mutex protection.
 * Returns 0 on success.
 */
static int asgn1_open(struct inode *inode,
                      struct file *file)
{
    int count;

    if (mutex_lock_interruptible(&asgn1_mutex))
        return -ERESTARTSYS;

    open_count++;
    count = open_count;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: device opened, count=%d\n",
            count);

    return 0;
}

/*
 * Release an open device file.
 * Returns 0 on success.
 */
static int asgn1_release(struct inode *inode,
                         struct file *file)
{
    pr_info("asgn1_driver: device closed\n");
    return 0;
}

/*
 * Copy data from the kernel buffer to userspace.
 * Updates file offset and read_count on success.
 * Returns bytes read, 0 for EOF, or negative errno.
 */
static ssize_t asgn1_read(struct file *file,
                          char __user *buf,
                          size_t len,
                          loff_t *offset)
{
    ssize_t bytes_read;

    if (mutex_lock_interruptible(&asgn1_mutex))
        return -ERESTARTSYS;

    if (*offset < 0) {
        mutex_unlock(&asgn1_mutex);
        return -EINVAL;
    }

    if (len == 0 || *offset >= buffer_len) {
        mutex_unlock(&asgn1_mutex);
        return 0;
    }

    len = min_t(size_t, len,
                (size_t)(buffer_len - *offset));

    if (copy_to_user(buf,
                     device_buffer + *offset,
                     len)) {
        mutex_unlock(&asgn1_mutex);
        return -EFAULT;
    }

    *offset += len;

    bytes_read = len;
    read_count++;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: read %zd bytes\n",
            bytes_read);

    return bytes_read;
}

/*
 * Write data into the internal device buffer.
 * Mode 0 prepends "[KERNEL]".
 * Mode 1 stores the original data.
 * Returns accepted input bytes or negative errno.
 */
static ssize_t asgn1_write(struct file *file,
                           const char __user *buf,
                           size_t len,
                           loff_t *offset)
{
    const char prefix[] = "[KERNEL]";
    const size_t prefix_len = sizeof(prefix) - 1;

    size_t data_len;
    size_t pos = 0;

    if (len == 0)
        return 0;

    if (mutex_lock_interruptible(&asgn1_mutex))
        return -ERESTARTSYS;

    if (current_mode == 0) {
        memcpy(device_buffer, prefix, prefix_len);
        pos = prefix_len;
    }

    data_len = min_t(size_t, len,
                     BUFFER_SIZE - 1 - pos);

    if (copy_from_user(device_buffer + pos,
                       buf, data_len)) {
        mutex_unlock(&asgn1_mutex);
        return -EFAULT;
    }

    buffer_len = pos + data_len;
    device_buffer[buffer_len] = '\0';

    write_count++;
    last_write_size = data_len;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: write %zu bytes\n",
            data_len);

    return data_len;
}

/* ===================================================
 * IOCTL INTERFACE
 * =================================================== */

/*
 * Process the four Assignment 01 ioctl commands.
 * cmd: ioctl command identifier.
 * arg: userspace argument pointer when required.
 * Returns 0 on success or negative errno on failure.
 */
static long asgn1_ioctl(struct file *file,
                        unsigned int cmd,
                        unsigned long arg)
{
    struct asgn1_stats stats;
    int mode;

    char version[ASGN1_VERSION_SIZE] = {0};

    switch (cmd) {

    case ASGN1_RESET_BUFFER:

        mutex_lock(&asgn1_mutex);

        memset(device_buffer, 0, BUFFER_SIZE);

        buffer_len = 0;
        write_count = 0;
        read_count = 0;
        last_write_size = 0;

        mutex_unlock(&asgn1_mutex);

        pr_info("asgn1_driver: RESET_BUFFER\n");

        return 0;

    case ASGN1_GET_STATS:

        mutex_lock(&asgn1_mutex);

        stats.open_count = open_count;
        stats.write_count = write_count;
        stats.read_count = read_count;
        stats.buffer_len = buffer_len;
        stats.last_write_size = last_write_size;

        mutex_unlock(&asgn1_mutex);

        if (copy_to_user((void __user *)arg,
                         &stats, sizeof(stats)))
            return -EFAULT;

        pr_info("asgn1_driver: GET_STATS\n");

        return 0;

    case ASGN1_SET_MODE:

        if (copy_from_user(&mode,
                           (void __user *)arg,
                           sizeof(mode)))
            return -EFAULT;

        if (mode != 0 && mode != 1)
            return -EINVAL;

        mutex_lock(&asgn1_mutex);
        current_mode = mode;
        mutex_unlock(&asgn1_mutex);

        pr_info("asgn1_driver: SET_MODE=%d\n",
                mode);

        return 0;

    case ASGN1_GET_VERSION:

        strscpy(version,
                "asgn1_driver v1.0 - SE205121",
                sizeof(version));

        if (copy_to_user((void __user *)arg,
                         version, sizeof(version)))
            return -EFAULT;

        pr_info("asgn1_driver: GET_VERSION\n");

        return 0;

    default:

        pr_warn("asgn1_driver: invalid ioctl command\n");

        return -ENOTTY;
    }
}

/* ===================================================
 * FILE OPERATIONS TABLE
 * =================================================== */

static const struct file_operations asgn1_fops = {
    .owner          = THIS_MODULE,
    .open           = asgn1_open,
    .release        = asgn1_release,
    .read           = asgn1_read,
    .write          = asgn1_write,
    .unlocked_ioctl = asgn1_ioctl,
};

/* ===================================================
 * MODULE INIT
 * =================================================== */

/*
 * Register the character device and create its
 * sysfs and procfs interfaces.
 * Returns 0 on success or negative errno.
 */
static int __init asgn1_init(void)
{
    int ret;

    pr_info("asgn1_driver: initializing\n");

    ret = register_chrdev(major_number,
                          DEVICE_NAME,
                          &asgn1_fops);

    if (ret < 0) {
        pr_err("asgn1_driver: register_chrdev failed: %d\n",
               ret);
        return ret;
    }

    asgn1_class = class_create(THIS_MODULE,
                               CLASS_NAME);

    if (IS_ERR(asgn1_class)) {
        ret = PTR_ERR(asgn1_class);
        goto err_unregister;
    }

    asgn1_device = device_create(asgn1_class,
                                 NULL,
                                 MKDEV(major_number, 0),
                                 NULL,
                                 DEVICE_NAME);

    if (IS_ERR(asgn1_device)) {
        ret = PTR_ERR(asgn1_device);
        goto err_class;
    }

    ret = sysfs_create_group(&asgn1_device->kobj,
                             &asgn1_attr_group);

    if (ret) {
        pr_err("asgn1_driver: sysfs creation failed\n");
        goto err_device;
    }

    sysfs_group_created = true;

    proc_entry = proc_create("asgn1_info",
                             0444,
                             NULL,
                             &asgn1_proc_fops);

    if (!proc_entry) {
        ret = -ENOMEM;
        pr_err("asgn1_driver: procfs creation failed\n");
        goto err_sysfs;
    }

    pr_info("asgn1_driver: loaded, major=%d\n",
            major_number);

    return 0;

err_sysfs:
    sysfs_remove_group(&asgn1_device->kobj,
                       &asgn1_attr_group);
    sysfs_group_created = false;

err_device:
    device_destroy(asgn1_class,
                   MKDEV(major_number, 0));

err_class:
    class_destroy(asgn1_class);

err_unregister:
    unregister_chrdev(major_number, DEVICE_NAME);

    return ret;
}

/* ===================================================
 * MODULE EXIT
 * =================================================== */

/*
 * Remove all registered interfaces and resources.
 */
static void __exit asgn1_exit(void)
{
    if (proc_entry)
        proc_remove(proc_entry);

    if (sysfs_group_created)
        sysfs_remove_group(&asgn1_device->kobj,
                           &asgn1_attr_group);

    device_destroy(asgn1_class,
                   MKDEV(major_number, 0));

    class_destroy(asgn1_class);

    unregister_chrdev(major_number, DEVICE_NAME);

    pr_info("asgn1_driver: module unloaded\n");
}

module_init(asgn1_init);
module_exit(asgn1_exit);
