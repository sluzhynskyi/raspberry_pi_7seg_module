#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

#define segA            2
#define segB            3
#define segC            4
#define segD            17
#define segE            27
#define segF            22
#define segG            10

#define DEVICE_NAME     "seven_seg" /* name that will be assigned to this device in /dev fs */
#define BUF_SIZE        512
#define NUM_COM         4 /* number of commands that this driver support */
#define MINORS_NUM      1

static int seven_seg_open(struct inode *inode, struct file *file_pointer);
static int seven_seg_release(struct inode *inode, struct file *file_pointer);
static ssize_t seven_seg_read (struct file *file_pointer, char *buf, size_t count, loff_t *f_pos);
static ssize_t seven_seg_write (struct file *file_pointer, const char *buf, size_t count, loff_t *f_pos);


static struct file_operations seven_seg_fops =
{
    .owner = THIS_MODULE,
    .open = seven_seg_open,
    .release = seven_seg_release,
    .read = seven_seg_read,
    .write = seven_seg_write,
};


struct seven_seg_dev
{
    struct cdev cdev;
    char display_number;
};


static int seven_seg_init(void);
static void seven_seg_exit(void);


struct seven_seg_dev *seven_seg_dev_pointer;

static dev_t first;

static struct class *seven_seg_class;


static int seven_seg_open (struct inode *inode, struct file *file_pointer)
{
    struct seven_seg_dev *seven_seg_dev_pointer;

    seven_seg_dev_pointer = container_of(inode->i_cdev, struct seven_seg_dev, cdev);

    file_pointer->private_data = seven_seg_dev_pointer;

    return 0;
}

static int seven_seg_release (struct inode *inode, struct file *file_pointer)
{
    file_pointer->private_data = NULL;

    return 0;
}

static ssize_t seven_seg_read ( struct file *file_pointer, char *buf, size_t count, loff_t *f_pos)
{

    ssize_t retval;
    char byte;
    struct seven_seg_dev *seven_seg_dev_pointer = file_pointer->private_data;

    for (retval = 0; retval < count; ++retval)
    {
        byte = '0' + seven_seg_dev_pointer->display_number;

        if(put_user(byte, buf+retval))
            break;
    }
    return retval;
}

static ssize_t seven_seg_write ( struct file *file_pointer, const char *buf, size_t count, loff_t *f_pos)
{
    unsigned int gpio, len = 0;
    char kbuf[BUF_SIZE];
    struct gpio_lkm_dev *gpio_lkm_devp = filp->private_data;

    /* get gpio device minor number
    */
    gpio = iminor(filp->f_path.dentry->d_inode);

    len = count < BUF_SIZE ? count-1 : BUF_SIZE-1;

    /* one more special kernel macro to copy data between
     * user space memory and kernel space memory
     */
    if(raw_copy_from_user(kbuf, buf, len) != 0)
        return -EFAULT;

    kbuf[len] = '\0';

    printk(KERN_INFO "[GPIO_LKM] - Got request from user: %s\n", kbuf);

    /* perform a switch on recieved command value
     * to determine, what request is received
     */
    switch(which_command(kbuf))
    {
    case set_in:
    {
        if (gpio_lkm_devp->dir != in)
        {
            printk(KERN_INFO "[GPIO_LKM] - Set GPIO%d direction: input\n", gpio);
            /* set direction input */
            gpio_direction_input(gpio);
            /* store state in device struct */
            gpio_lkm_devp->dir = in;
        }
        break;
    }
    case set_out:
    {
        if (gpio_lkm_devp->dir != out)
        {
            printk(KERN_INFO " Set GPIO%d direction: ouput\n", gpio);
            /* set direction output and low level */
            gpio_direction_output(gpio, low);
            /* store state in device struct */
            gpio_lkm_devp->dir = out;
            gpio_lkm_devp->state = low;
        }
        break;
    }
    case set_high:
    {
        if (gpio_lkm_devp->dir == in)
        {
            printk("[GPIO_LKM] - Cannot set GPIO %d, direction: input\n", gpio);
            return -EPERM;
        }
        else
        {
            printk("[GPIO_LKM] - got to set_high\n");
            gpio_set_value(gpio, high);
            gpio_lkm_devp->state = high;
        }
        break;
    }
    case set_low:
        if (gpio_lkm_devp->dir == in)
        {
            printk("[GPIO_LKM] - Cannot set GPIO %d, direction: input\n", gpio);
            return -EPERM;
        }
        else
        {
            gpio_set_value(gpio, low);
            gpio_lkm_devp->state = low;
        }
        break;
    default:
            printk(KERN_ERR "[GPIO_LKM] - Invalid input value\n");
            return -EINVAL;
        break;

    }

    *f_pos += count;
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

    if ((seven_seg_class = class_create( THIS_MODULE, DEVICE_NAME)) == NULL)
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


    seven_seg_dev_pointer->display_number = 0;
    seven_seg_dev_pointer->cdev.owner = THIS_MODULE;

    cdev_init(&seven_seg_dev_pointer->cdev, &seven_seg_fops);


    if ((ret = cdev_add( &seven_seg_dev_pointer->cdev, first, 1)))
    {
        printk (KERN_ALERT "[SEVEN_SEG] - Error %d adding cdev\n", ret);
        device_destroy (seven_seg_class, first);
        class_destroy(seven_seg_class);
        unregister_chrdev_region(first, MINORS_NUM);
        return ret;
    }

    if (device_create( seven_seg_class, NULL, first, NULL, "seven_seg") == NULL)
    {
        class_destroy(seven_seg_class);
        unregister_chrdev_region(first, MINORS_NUM);

        return -1;
    }

    printk("[SEVEN_SEG] - Driver initialized\n");
    
    return 0;
}

static void __exit seven_seg_exit(void)
{
    unregister_chrdev_region(first, MINORS_NUM);

    kfree(seven_seg_dev_pointer);

    device_destroy ( seven_seg_class, MKDEV(MAJOR(first), MINOR(first)));

    class_destroy(seven_seg_class);

    printk(KERN_INFO "[SEVEN_SEG] - Raspberry Pi 7-segment driver removed\n");
}


module_init(seven_seg_init);
module_exit(seven_seg_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roman Okhrimenko <mrromanjoe@gmail.com>");
MODULE_DESCRIPTION("GPIO Loadable Kernel Module - Linux device driver for Raspberry Pi");
