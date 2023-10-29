/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Vidhya. PL");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev_struct;
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    dev_struct = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev_struct;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = 0;
    ssize_t bytes_unread = 0;
    ssize_t read_bytes = 0;
    ssize_t offset_val = 0;

    struct aesd_dev *device_struct;

    struct aesd_buffer_entry *read_buffer_entry = NULL;


    printk(KERN_DEBUG "read %zu bytes with offset %lld",count,*f_pos);
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    device_struct = (struct aesd_dev*) filp->private_data;

    if(filp == NULL || buf == NULL || f_pos == NULL)
    {
        return -EFAULT; 
    }

    if(mutex_lock_interruptible(&device_struct->mutex_lock))
    {
        printk(KERN_ALERT "Mutex lock failed");
        return -ERESTARTSYS; 
    }

    read_buffer_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(device_struct->cbuff), *f_pos, &offset_val);
    if(read_buffer_entry == NULL)
    {
        mutex_unlock(&(device_struct->mutex_lock));
        return retval;
    }

    if(count > (read_buffer_entry->size - offset_val))
    {
        read_bytes = read_buffer_entry->size - offset_val;
    }
    else
    {
        read_bytes = count;
    }

    bytes_unread = copy_to_user(buf, (read_buffer_entry->buffptr + offset_val), read_bytes);

    if(bytes_unread == 0)
    {
        printk(KERN_ALERT "successfully copied %ld bytes\n", read_bytes);
    }
    else
    {
        printk(KERN_ALERT "Failed to copied %ld bytes\n", read_bytes);
        return -EFAULT;
    }
    retval = read_bytes;
    *f_pos += read_bytes;
    mutex_unlock(&(device_struct->mutex_lock));
    return retval;
}

static ssize_t write_to_buffer(struct aesd_dev *device_struct, const char __user *buf, size_t count) 
{
    ssize_t bytes_unwritten = copy_from_user((void *)(device_struct->buffer_entry.buffptr + device_struct->buffer_entry.size), buf, count);
    device_struct->buffer_entry.size += (count - bytes_unwritten);
    return (count - bytes_unwritten);
}

static int handle_circular_buffer(struct aesd_dev *device_struct, const char *current_entry) 
{
    if (current_entry) 
    {
        kfree(current_entry);
    }
    device_struct->buffer_entry.buffptr = NULL;
    device_struct->buffer_entry.size = 0;
    return 0;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    int buff_error;
    int i;

    struct aesd_dev *device_struct;
    const char *current_entry = NULL;

    device_struct = (struct aesd_dev*) filp->private_data;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */

    if(mutex_lock_interruptible(&(device_struct->mutex_lock)))
    {
        printk(KERN_ALERT "Mutex lock failed");
        return -ERESTARTSYS;
    }

    if (device_struct->buffer_entry.size == 0) 
    {
        device_struct->buffer_entry.buffptr = kmalloc(count * sizeof(char), GFP_KERNEL);
    } 
    else
    {
        device_struct->buffer_entry.buffptr = krealloc(device_struct->buffer_entry.buffptr, (device_struct->buffer_entry.size + count) * sizeof(char), GFP_KERNEL);
    }

    if (device_struct->buffer_entry.buffptr == NULL) 
    {
        mutex_unlock(&device_struct->mutex_lock);
        return -ENOMEM;
    }

    retval = write_to_buffer(device_struct, buf, count);

    for (i = 0; i < device_struct->buffer_entry.size; i++) 
    {
        if (device_struct->buffer_entry.buffptr[i] == '\n') 
        {
            current_entry = aesd_circular_buffer_add_entry(&device_struct->cbuff, &device_struct->buffer_entry);
            buff_error = handle_circular_buffer(device_struct, current_entry);
            if (buff_error) {
                mutex_unlock(&device_struct->mutex_lock);
                return buff_error;
            }
            break;
        }
    }
    mutex_unlock(&device_struct->mutex_lock);
    return retval;
}

struct file_operations aesd_fops = {
.owner =    THIS_MODULE,
.read =     aesd_read,
.write =    aesd_write,
.open =     aesd_open,
.release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device.mutex_lock);
    aesd_circular_buffer_init(&aesd_device.cbuff);
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    struct aesd_buffer_entry *entry = NULL;
    uint8_t index = 0;
    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    kfree(aesd_device.buffer_entry.buffptr);
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.cbuff, index)
    {
        if(entry->buffptr != NULL)
        {
            kfree(entry->buffptr);
        }
    }

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
