#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <asm/uaccess.h>

#define MAX_MEM (2048 * 4096)

MODULE_AUTHOR("Kologermansky Egor, 796");
MODULE_LICENSE("No license, just fun");
MODULE_DESCRIPTION("Best r/w module ever");

#define SUCCESS 0
#define DEVICE_NAME "miniFSdriver"
#define UNIQUE_TAG "[FILE DRIVER] "

static int device_open( struct inode *, struct file * );
static int device_release( struct inode *, struct file * );
static ssize_t device_read( struct file *, char *, size_t, loff_t * );
static ssize_t device_write( struct file *, const char *, size_t, loff_t * );
static loff_t device_llseek(struct file *, loff_t, int);

static int major_number;
static volatile int is_device_open = 0;
static char mem[MAX_MEM];
static loff_t curr_offset;

static struct file_operations fops =
{
    .llseek = device_llseek,
    //.llseek = NULL,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

static int __init test_init(void)
{
    printk(UNIQUE_TAG "My driver loaded!\n");
    
    major_number = register_chrdev(0, DEVICE_NAME, &fops);

    if (major_number < 0)
    {
        printk(UNIQUE_TAG "Registering the character device failed with %d\n", -major_number);
        return major_number;
    }
    
    printk(UNIQUE_TAG "Test module is loaded!\n");

    printk(UNIQUE_TAG "Please, create a dev file with 'mknod /dev/"DEVICE_NAME" c %d 0'.\n", major_number);

    return SUCCESS;
}

static void __exit test_exit(void)
{
    unregister_chrdev(major_number, DEVICE_NAME);

    printk(UNIQUE_TAG "Test module is unloaded!\n");
}

module_init( test_init );
module_exit( test_exit );

static int device_open(struct inode *inode, struct file *file)
{
    printk(UNIQUE_TAG "Open device\n");

    if (is_device_open)
    {
        return -EBUSY;
    }
  
    ++is_device_open;

    return SUCCESS;
}

static int device_release( struct inode *inode, struct file *file )
{
    printk(UNIQUE_TAG "Close device\n");
    --is_device_open;
    return SUCCESS;
}

static ssize_t device_write( struct file *filp, const char __user *buff, size_t len, loff_t * offset)
{
    loff_t enough_len = MAX_MEM - curr_offset;
    size_t i = 0;

    printk(UNIQUE_TAG "Write buff with len - %lu and offset - %lld\n", len, *offset);

    // copy_from_user

    if (enough_len < len)
    {
        len = enough_len;
    }

    while(i < len)
    {
        if (raw_copy_from_user(mem + curr_offset + i, buff + i, 1) != 0)
        {
            return -EFAULT;
        }
        ++i;
    }

    return len;
}

static ssize_t device_read( struct file *filp, char __user *buff, size_t len, loff_t * offset)
{
    loff_t enough_len = MAX_MEM - curr_offset;
    size_t i = 0;

    printk(UNIQUE_TAG "Read buff with len - %lu and offset - %lld\n", len, *offset);

    // copy_to_user

    if (enough_len < len)
    {
        len = enough_len;
    }

    while(i < len)
    {
        if (raw_copy_to_user(buff + i, mem + curr_offset + i, 1) != 0)
        {
            return -EFAULT;
        }
        ++i;
    }

    return len;
}

static loff_t device_llseek(struct file *filp, loff_t offset, int type)
{
    printk(UNIQUE_TAG "Seek to offset - %lld type - %d\n", offset, type);
    
    curr_offset = offset;

    return offset;
}