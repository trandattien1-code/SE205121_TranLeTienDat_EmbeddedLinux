
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tran Le Tien Dat - SE205121");
MODULE_DESCRIPTION("Assignment 01 Task 2 - Atomic Counter Misc Device");
MODULE_VERSION("1.0");

#define DEVICE_NAME "counter"
#define PROC_NAME   "counter_info"
#define INPUT_SIZE  32

static atomic_t counter = ATOMIC_INIT(0);
static atomic_t total_reads = ATOMIC_INIT(0);
static atomic_t total_writes = ATOMIC_INIT(0);

static DEFINE_MUTEX(counter_mutex);

static struct proc_dir_entry *counter_proc_entry;

/*
 * Read the counter value and increment it once.
 * Returns a decimal string ending with newline.
 */
static ssize_t counter_read(struct file *file,
                            char __user *buf,
                            size_t len,
                            loff_t *ppos)
{
    char output[32];
    int value;
    int output_len;
    ssize_t ret;

    if (*ppos != 0)
        return 0;

    if (mutex_lock_interruptible(&counter_mutex))
        return -ERESTARTSYS;

    value = atomic_read(&counter);
    output_len = scnprintf(output, sizeof(output),
                           "%d\n", value);

    if (len < output_len) {
        mutex_unlock(&counter_mutex);
        return -EINVAL;
    }

    if (copy_to_user(buf, output, output_len)) {
        mutex_unlock(&counter_mutex);
        return -EFAULT;
    }

    atomic_inc(&counter);
    atomic_inc(&total_reads);

    *ppos += output_len;
    ret = output_len;

    mutex_unlock(&counter_mutex);

    pr_info("counter_driver: read=%d\n", value);

    return ret;
}

/*
 * Parse a non-negative ASCII integer from userspace.
 * Set the counter to that value.
 */
static ssize_t counter_write(struct file *file,
                             const char __user *buf,
                             size_t len,
                             loff_t *ppos)
{
    char input[INPUT_SIZE];
    int value;
    int ret;

    if (len == 0 || len >= INPUT_SIZE)
        return -EINVAL;

    if (copy_from_user(input, buf, len))
        return -EFAULT;

    input[len] = '\0';

    ret = kstrtoint(strim(input), 10, &value);

    if (ret < 0 || value < 0)
        return -EINVAL;

    if (mutex_lock_interruptible(&counter_mutex))
        return -ERESTARTSYS;

    atomic_set(&counter, value);
    atomic_inc(&total_writes);

    mutex_unlock(&counter_mutex);

    pr_info("counter_driver: set counter=%d\n", value);

    return len;
}

/*
 * Display counter statistics through procfs.
 */
static int counter_proc_show(struct seq_file *m, void *v)
{
    int value;
    int reads;
    int writes;

    mutex_lock(&counter_mutex);

    value = atomic_read(&counter);
    reads = atomic_read(&total_reads);
    writes = atomic_read(&total_writes);

    mutex_unlock(&counter_mutex);

    seq_printf(m, "Counter value : %d\n", value);
    seq_printf(m, "Total reads   : %d\n", reads);
    seq_printf(m, "Total writes  : %d\n", writes);

    return 0;
}

/* Open the procfs statistics file. */
static int counter_proc_open(struct inode *inode,
                             struct file *file)
{
    return single_open(file, counter_proc_show, NULL);
}

static const struct proc_ops counter_proc_ops = {
    .proc_open    = counter_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static const struct file_operations counter_fops = {
    .owner = THIS_MODULE,
    .read  = counter_read,
    .write = counter_write,
};

static struct miscdevice counter_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DEVICE_NAME,
    .fops  = &counter_fops,
    .mode  = 0666,
};

/*
 * Register the misc device and create procfs.
 * Returns 0 on success or negative errno.
 */
static int __init counter_init(void)
{
    int ret;

    atomic_set(&counter, 0);
    atomic_set(&total_reads, 0);
    atomic_set(&total_writes, 0);

    ret = misc_register(&counter_misc_device);

    if (ret) {
        pr_err("counter_driver: misc_register failed\n");
        return ret;
    }

    counter_proc_entry = proc_create(PROC_NAME, 0444,
                                     NULL, &counter_proc_ops);

    if (!counter_proc_entry) {
        misc_deregister(&counter_misc_device);
        return -ENOMEM;
    }

    pr_info("counter_driver: loaded, major=10, minor=%d\n",
            counter_misc_device.minor);

    return 0;
}

/*
 * Remove procfs and deregister the misc device.
 */
static void __exit counter_exit(void)
{
    if (counter_proc_entry)
        proc_remove(counter_proc_entry);

    misc_deregister(&counter_misc_device);

    pr_info("counter_driver: unloaded\n");
}

module_init(counter_init);
module_exit(counter_exit);
