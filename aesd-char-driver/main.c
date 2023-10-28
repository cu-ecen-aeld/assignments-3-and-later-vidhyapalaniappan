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
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Vidhya Palaniappan"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

static struct write_command_entry aesd_write_commands[MAX_WRITE_COMMANDS];
static int aesd_write_commands_count = 0;
static int aesd_write_commands_start = 0;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *dev_struct;
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

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    mutex_lock(&aesd_device.cbuff.mutex_lock);
    int current_entry = aesd_write_commands_start;
    size_t current_offset = *f_pos;

        while (count > 0 && write_commands_count > 0) {
        struct write_command_entry *entry = &write_commands[current_entry];

        // Calculate how much data to copy from the current entry
        size_t to_copy = min(count, entry->length - current_offset);

        if (copy_to_user(buf, entry->content + current_offset, to_copy)) {
            retval = -EFAULT;
            break;
        }

        // Update count, offset, and file position
        count -= to_copy;
        buf += to_copy;
        current_offset = 0;
        *f_pos += to_copy;

        // Move to the next entry if more data is needed
        current_entry = (current_entry + 1) % MAX_WRITE_COMMANDS;
        if (current_entry == write_commands_start) {
            // We've looped through all entries, break the loop
            break;
        }

        mutex_unlock(&aesd_device.cbuff.mutex_lock);
    }

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */

    char *command;
    size_t command_len;

        // Lock the circular buffer to ensure safe access
    mutex_lock(&aesd_device.cbuff.mutex_lock);

    // Allocate memory for the write command within the circular buffer
    command = kmalloc(count, GFP_KERNEL);
    if (!command) {
        goto out_unlock;
    }

    // Copy the write command from user space
    if (copy_from_user(command, buf, count)) {
        retval = -EFAULT;
        goto out_free;
    }

    // Search for a terminated command and append the current write if necessary
    int current_entry = (aesd_write_commands_start + aesd_write_commands_count) % MAX_WRITE_COMMANDS;

    if (write_commands_count > 0 && write_commands[current_entry].content) {
        size_t available_space = PAGE_SIZE - write_commands[current_entry].length;
        if (available_space >= count) {
            // There's enough space to append the current write to the last entry
            memcpy(write_commands[current_entry].content + write_commands[current_entry].length, command, count);
            write_commands[current_entry].length += count;
            retval = count;
        }
    }

    // If the current entry is not terminated, create a new entry
    if (retval == -ENOMEM) {
        // Free memory if there are too many write command entries
        if (write_commands_count == MAX_WRITE_COMMANDS) {
            kfree(write_commands[write_commands_start].content);
            write_commands[write_commands_start].content = NULL;
            write_commands_start = (write_commands_start + 1) % MAX_WRITE_COMMANDS;
        }

        // Allocate memory for the new entry
        write_commands[current_entry].content = command;
        write_commands[current_entry].length = count;
        write_commands_count++;
        retval = count;
    }

    out_free:
    if (retval != count) {
        kfree(command);
    }
    out_unlock:
    // Unlock the circular buffer
    mutex_unlock(&aesd_device.cbuff.mutex_lock);

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
	
    mutex_init(&aesd_device.lock);
    ased_circular_buffer_init(&aesd_device.circular_buffer);	
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
