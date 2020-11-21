#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

#define DEVICE_NAME     "seven_seg" /* name that will be assigned to this device in /dev fs */
#define BUF_SIZE        512
#define NUM_COM         4 /* number of commands that this driver support */
#define MINORS_NUM      1


const int pins[]{
        2, // segA
        3, // segB
        4, // segC
        17, // segD
        27, // segE
        22, // segF
        10, // segG
};
const int masks[] = {
//      A     B     C     D     E     F     G
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,
};
const int digit_bitmap[] = {
//      0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x67, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71,
};

static int seven_seg_open(struct inode *inode, struct file *file_pointer);

static int seven_seg_release(struct inode *inode, struct file *file_pointer);

static ssize_t seven_seg_read(struct file *file_pointer, char *buf, size_t count, loff_t *f_pos);

static ssize_t seven_seg_write(struct file *file_pointer, const char *buf, size_t count, loff_t *f_pos);


static struct file_operations seven_seg_fops = {
        .owner = THIS_MODULE,
        .open = seven_seg_open,
        .release = seven_seg_release,
        .read = seven_seg_read,
        .write = seven_seg_write,
};


struct seven_seg_dev {
    struct cdev cdev;
    char display_number;
};


static int seven_seg_init(void);

static void seven_seg_exit(void);


struct seven_seg_dev *seven_seg_dev_pointer;

static dev_t first;

static struct class *seven_seg_class;


void set_to_screen(char *c) {
    int val = digit_bitmap[*c - 48];
    for (int seg = segA; seg <= segG; ++seg) {
        gpio = seg;
        gpio_set_value(gpio, low);
    }
    int masks_len = sizeof masks / sizeof *masks;
    for (int i = 0; i < masks_len; ++i) {
        if ((masks[i] & val) == masks[i]) {
            gpio_set_value(pins[i], high);
        }
    }

}

static int seven_seg_open(struct inode *inode, struct file *file_pointer) {
    struct seven_seg_dev *seven_seg_dev_pointer;

    seven_seg_dev_pointer = container_of(inode->i_cdev,
    struct seven_seg_dev, cdev);

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
        byte = '0' + seven_seg_dev_pointer->display_number;

        if (put_user(byte, buf + retval))
            break;
    }
    return retval;
}

static ssize_t seven_seg_write(struct file *file_pointer, char *buf, size_t count, loff_t *f_pos) {
    char byte;
    struct gpio_lkm_dev *gpio_lkm_devp = filp->private_data;


    if (get_user(&byte, buf) != 0)
        return -EFAULT;


    printk(KERN_INFO
    "[SEVEN_SEG] - Got request from user: %c\n", byte);
    set_to_screen(&byte)

    * f_pos += count;
    return count;

}


static int __init

seven_seg_init(void) {
    int ret = 0;

    if (alloc_chrdev_region(&first, 0, MINORS_NUM, DEVICE_NAME) < 0) {
        printk(KERN_DEBUG
        "Cannot register device\n");
        return -1;
    }

    if ((seven_seg_class = class_create(THIS_MODULE, DEVICE_NAME)) == NULL) {
        printk(KERN_DEBUG
        "Cannot create class %s\n", DEVICE_NAME);
        unregister_chrdev_region(first, MINORS_NUM);
        return -EINVAL;
    }

    seven_seg_dev_pointer = kmalloc(sizeof(struct seven_seg_dev), GFP_KERNEL);
    if (!seven_seg_dev_pointer) {
        printk(KERN_DEBUG
        "[SEVEN_SEG]Bad kmalloc\n");
        return -ENOMEM;
    }


    seven_seg_dev_pointer->display_number = 0;
    seven_seg_dev_pointer->cdev.owner = THIS_MODULE;

    cdev_init(&seven_seg_dev_pointer->cdev, &seven_seg_fops);


    if ((ret = cdev_add(&seven_seg_dev_pointer->cdev, first, 1))) {
        printk(KERN_ALERT
        "[SEVEN_SEG] - Error %d adding cdev\n", ret);
        device_destroy(seven_seg_class, first);
        class_destroy(seven_seg_class);
        unregister_chrdev_region(first, MINORS_NUM);
        return ret;
    }

    if (device_create(seven_seg_class, NULL, first, NULL, "seven_seg") == NULL) {
        class_destroy(seven_seg_class);
        unregister_chrdev_region(first, MINORS_NUM);

        return -1;
    }

    printk("[SEVEN_SEG] - Driver initialized\n");

    return 0;
}

static void __exit

seven_seg_exit(void) {
    unregister_chrdev_region(first, MINORS_NUM);

    kfree(seven_seg_dev_pointer);

    device_destroy(seven_seg_class, MKDEV(MAJOR(first), MINOR(first)));

    class_destroy(seven_seg_class);

    printk(KERN_INFO
    "[SEVEN_SEG] - Raspberry Pi 7-segment driver removed\n");
}


module_init(seven_seg_init);
module_exit(seven_seg_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roman Okhrimenko <mrromanjoe@gmail.com>");
MODULE_DESCRIPTION("GPIO Loadable Kernel Module - Linux device driver for Raspberry Pi");
