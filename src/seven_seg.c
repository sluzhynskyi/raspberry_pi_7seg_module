#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include "seven_seg.h"

#define DEVICE_NAME     "seven_seg" /* name that will be assigned to this device in /dev fs */
#define MINORS_NUM      1
#define DIGITS_NUM      1

static int seven_seg_open(struct inode *inode, struct file *file_pointer);
static int seven_seg_release(struct inode *inode, struct file *file_pointer);
static ssize_t seven_seg_read(struct file *file_pointer, char *buf, size_t count, loff_t *f_pos);
static ssize_t seven_seg_write(struct file *file_pointer, const char *buf, size_t count, loff_t *f_pos);

static int seven_seg_init(void);
static void seven_seg_exit(void);

struct seven_seg_dev {
    struct cdev cdev;
    char display_value;
};

static struct file_operations seven_seg_fops = {
        .owner = THIS_MODULE,
        .open = seven_seg_open,
        .release = seven_seg_release,
        .read = seven_seg_read,
        .write = seven_seg_write,
};

struct seven_seg_dev *seven_seg_dev_pointer;
static char *msg = NULL;
static dev_t first;



//
// Class Attributes section
//
static ssize_t display_value_show(struct class *cls, struct class_attribute *attr, char *buf){
    printk(KERN_INFO "[SEVEN_SEG] read display value - %c\n", '0'+seven_seg_dev_pointer->display_value);
    return sprintf(buf, "%c\n", '0'+seven_seg_dev_pointer->display_value);
};
static ssize_t display_value_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count){
    if (buf[0]-48 >= 0 && buf[0]-48 < 10) {
        seven_seg_dev_pointer->display_value = buf[0] - 48;
        set_to_screen(buf);
    }
    return 1;
};
static CLASS_ATTR_RW(display_value);
static struct attribute *class_attr_attrs[] = { &class_attr_display_value.attr, NULL };
ATTRIBUTE_GROUPS(class_attr);

static struct class seven_seg_class = {
        .name = DEVICE_NAME,
        .owner = THIS_MODULE,
        .class_groups = class_attr_groups,
};


//
// Device Functions Section
//

static int seven_seg_open(struct inode *inode, struct file *file_pointer) {
    struct seven_seg_dev *seven_seg_dev_pointer;
    seven_seg_dev_pointer = container_of(inode->i_cdev, struct seven_seg_dev, cdev);
    file_pointer->private_data = seven_seg_dev_pointer;
    return 0;
}

static int seven_seg_release(struct inode *inode, struct file *file_pointer) {
    file_pointer->private_data = NULL;
    return 0;
}

static ssize_t seven_seg_read(struct file *file_pointer, char *buf, size_t count, loff_t *f_pos) {
    ssize_t retval;
    char byte;
    struct seven_seg_dev *seven_seg_dev_pointer = file_pointer->private_data;

    for (retval = 0; retval < count; ++retval) {
        byte = '0' + seven_seg_dev_pointer->display_value;
        if (put_user(byte, buf + retval))
            break;
    }
    return retval;
}

static ssize_t seven_seg_write(struct file *file_pointer, const char *buf, size_t count, loff_t *f_pos) {
    struct seven_seg_dev *seven_seg_dev_pointer = file_pointer->private_data;
    memset(msg, 0, DIGITS_NUM);
    if (copy_from_user(msg, buf, count) != 0)
        return -EFAULT;
    seven_seg_dev_pointer->display_value = msg[0]-48;
    set_to_screen(msg);
    f_pos += count;
    return count;
}


static int __init seven_seg_init(void)
{
    int ret = 0;

    if (alloc_chrdev_region(&first, 0, MINORS_NUM, DEVICE_NAME) < 0)
    {
        printk(KERN_DEBUG "Cannot register device\n");
        return -1;
    }

    if (class_register(&seven_seg_class) < 0)
    {
        printk(KERN_DEBUG "Cannot create class %s\n", DEVICE_NAME);
        unregister_chrdev_region(first, MINORS_NUM);
        return -EINVAL;
    }

    seven_seg_dev_pointer = kmalloc(sizeof(struct seven_seg_dev), GFP_KERNEL);
    if (!seven_seg_dev_pointer)
    {
        printk(KERN_DEBUG "[SEVEN_SEG]Bad kmalloc\n");
        return -ENOMEM;
    }

    seven_seg_dev_pointer->display_value = '1'-48;
    seven_seg_dev_pointer->cdev.owner = THIS_MODULE;

    cdev_init(&seven_seg_dev_pointer->cdev, &seven_seg_fops);

    if ((ret = cdev_add( &seven_seg_dev_pointer->cdev, first, 1)))
    {
        printk (KERN_ALERT "[SEVEN_SEG] - Error %d adding cdev\n", ret);
        device_destroy (&seven_seg_class, first);
        class_destroy(&seven_seg_class);
        unregister_chrdev_region(first, MINORS_NUM);
        return ret;
    }

    if (device_create( &seven_seg_class, NULL, first, NULL, "seven_seg") == NULL)
    {
        class_destroy(&seven_seg_class);
        unregister_chrdev_region(first, MINORS_NUM);
        return -1;
    }

    msg = (char *)kmalloc(DIGITS_NUM, GFP_KERNEL);
    printk("[SEVEN_SEG] - Driver initialized\n");
    return 0;
}

static void __exit seven_seg_exit(void)
{
    unregister_chrdev_region(first, MINORS_NUM);
    kfree(seven_seg_dev_pointer);
    device_destroy ( &seven_seg_class, MKDEV(MAJOR(first), MINOR(first)));
    class_destroy(&seven_seg_class);
    printk(KERN_INFO "[SEVEN_SEG] - Raspberry Pi 7-segment driver removed\n");
}

module_init(seven_seg_init);
module_exit(seven_seg_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marian Dubei, Danylo Sluzhynskyi, Taras Rumezhak");
MODULE_DESCRIPTION("GPIO Loadable Kernel Module - Linux device driver for Raspberry Pi");
