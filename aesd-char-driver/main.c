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
 * @modified Vidhya. PL
 * @modified on 2023-10-29
 * @reference and credits- aesd_ioctl func : https://github.com/cu-ecen-aeld/assignments-3-and-later-chchbuff/blob/main/aesd-char-driver/main.c
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"
#include "aesd_ioctl.h"

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Vidhya. PL");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev_struct; //pointer to reference the character device being opened
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    dev_struct = container_of(inode->i_cdev, struct aesd_dev, cdev); //from lecture slides
    filp->private_data = dev_struct; //associating the opened file with the device structure

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
    ssize_t retval = 0;  //variable to store the return value
    ssize_t bytes_unread = 0;  //variable to track the unread bytes
    ssize_t read_bytes = 0;  //variable to track the read bytes
    ssize_t offset_val = 0; ////variable to track the offset byte

    struct aesd_dev *device_struct;  // used to reference the device structure associated with the character device

    struct aesd_buffer_entry *read_buffer_entry = NULL; //This pointer to reference a buffer entry within a circular buffer.


    printk(KERN_DEBUG "read %zu bytes with offset %lld",count,*f_pos);
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    device_struct = (struct aesd_dev*) filp->private_data;  //to access the device-specific data associated with the file

    if(filp == NULL || buf == NULL || f_pos == NULL) //checking if any of the essential pointers is NULL
    {
        return -EFAULT; 
    }

    if(mutex_lock_interruptible(&device_struct->mutex_lock)) //checking if obtaining a mutex is interrupted by a signal
    {
        printk(KERN_ALERT "Mutex lock failed");
        return -ERESTARTSYS; 
    }

    //to find a buffer entry within a circular buffer associated with the device using f_pos
    read_buffer_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(device_struct->cbuff), *f_pos, &offset_val);
    
    if(read_buffer_entry == NULL) //checking if the buffer entry is found
    {
        mutex_unlock(&(device_struct->mutex_lock));
        return retval;
    }

    //checking if the requested count is greater than the remaining bytes in the buffer entry
    if(count > (read_buffer_entry->size - offset_val))
    {
        read_bytes = read_buffer_entry->size - offset_val;  //if true, setting the read_byes to remaining bytes
    }
    else
    {
        read_bytes = count;
    }

    //copying data from the kernel spacevto user space
    bytes_unread = copy_to_user(buf, (read_buffer_entry->buffptr + offset_val), read_bytes);

    if(bytes_unread == 0)
    {
        printk(KERN_ALERT "successfully copied %ld bytes\n", read_bytes);
    }
    else
    {
        printk(KERN_ALERT "Failed to copied %ld bytes\n", read_bytes);
        mutex_unlock(&(device_struct->mutex_lock)); //releasing the mutex
        return -EFAULT;
    }
    retval = read_bytes;  //number of bytes successfully read
    *f_pos += read_bytes; //updating the file position reflect the new position after the read operation
    mutex_unlock(&(device_struct->mutex_lock)); //releasing the mutex
    return retval;
}

//The function is responsible for writing data from user space to a kernel buffer
static ssize_t write_to_buffer(struct aesd_dev *device_struct, const char __user *buf, size_t count) 
{
    //copying data from user space to kernel space
    ssize_t bytes_unwritten = copy_from_user((void *)(device_struct->buffer_entry.buffptr + device_struct->buffer_entry.size), buf, count);
    //updating the size of the kernel buffer
    device_struct->buffer_entry.size += (count - bytes_unwritten);
    return (count - bytes_unwritten); //returning the number of bytes successfully written to the kernel buffer
}

//function for managing a circular buffer associated with device driver
static int handle_circular_buffer(struct aesd_dev *device_struct, const char *current_entry) 
{
    if (current_entry)  //checking if there is a valid current entry in the circular buffer
    {
        kfree(current_entry); //free the memory associated with the current_entry
    }
    device_struct->buffer_entry.buffptr = NULL; //clearing the buffer pointer, indicating that the circular buffer is now empty.
    device_struct->buffer_entry.size = 0; //indicating that the size of the circular buffer is now zero, effectively empty.
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

    //checking whether the size of the circular buffer is zero, indicating that it's empty
    if (device_struct->buffer_entry.size == 0) 
    {
        device_struct->buffer_entry.buffptr = kmalloc(count * sizeof(char), GFP_KERNEL);
    } 
    else //If the buffer is not empty, this block is executed
    {
        device_struct->buffer_entry.buffptr = krealloc(device_struct->buffer_entry.buffptr, (device_struct->buffer_entry.size + count) * sizeof(char), GFP_KERNEL);
    }

    if (device_struct->buffer_entry.buffptr == NULL) //checking whether memory allocation or reallocation was successful
    {
        mutex_unlock(&device_struct->mutex_lock);
        return -ENOMEM;
    }

    retval = write_to_buffer(device_struct, buf, count);

    for (i = 0; i < device_struct->buffer_entry.size; i++) 
    {
        if (device_struct->buffer_entry.buffptr[i] == '\n')  //checking if a newline character is encountered in the circular buffer
        {
            current_entry = (const char *)aesd_circular_buffer_add_entry(&device_struct->cbuff, &device_struct->buffer_entry);
            buff_error = handle_circular_buffer(device_struct, current_entry);
            if (buff_error)  //checking if an error occurred during the circular buffer handling process
            {
                mutex_unlock(&device_struct->mutex_lock);
                return buff_error;
            }
            break;
        }
    }
    mutex_unlock(&device_struct->mutex_lock);
    return retval;
}

/***************************************** A9 ***********************************************************/
loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    struct aesd_buffer_entry *buff_entry = NULL;  //declaring a pointer to a struct aesd_buffer_entry
    struct aesd_dev *device = filp->private_data; //extracting the private_data field from the struct file pointer filp
    loff_t seek_offset = 0; //to store the new file position
    loff_t seek_size = 0; //to accumulate the sizes of the aesd_buffer_entry structures.
    uint8_t i = 0; //loop counter
    
    if (filp == NULL)
    {
        return -EFAULT;
    }
    if (mutex_lock_interruptible(&device->mutex_lock) != 0)
    {
        return -ERESTARTSYS;
    }
    AESD_CIRCULAR_BUFFER_FOREACH(buff_entry,&aesd_device.cbuff,i) // iterating through the circular buffer
    {
        seek_size += buff_entry->size; //adding the size of each buff_entry into the seek_size variable
    }
    mutex_unlock(&device->mutex_lock);
    seek_offset = fixed_size_llseek(filp, offset, whence, seek_size); //calculating the new file position based on the offset, whence, and the total size of the file.
    return seek_offset;

}

static long file_offset_move(struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset)
{
    struct aesd_dev *device = filp->private_data; //extracting the private_data field from the struct file pointer filp
    struct aesd_buffer_entry *buff_entry = NULL;  //declaring a pointer to a struct aesd_buffer_entry
    long retval = 0; //return variable
    uint8_t i = 0; //loop counter

    if (filp == NULL)
    {
        return -EFAULT;
    }
    if (mutex_lock_interruptible(&device->mutex_lock) != 0)
    {
        return -ERESTARTSYS;
    }
    AESD_CIRCULAR_BUFFER_FOREACH(buff_entry,&aesd_device.cbuff,i); // iterating through the circular buffer
    if ((write_cmd > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) || (write_cmd > i) || (write_cmd_offset >= device->cbuff.entry[write_cmd].size))
    {
    	mutex_unlock(&device->mutex_lock);
    	return -EINVAL;
    }
    for (i = 0; i < write_cmd; i++)
    {
        filp->f_pos += device->cbuff.entry[i].size; //moving the file position within the circular buffer.
    }
    filp->f_pos += write_cmd_offset; //setting the final file position.
    mutex_unlock(&device->mutex_lock);
    return retval;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_seekto seeker; //variable to store data provided by the user through the arg parameter.
    long retval = 0; //return variable

    if (filp == NULL)
    {
        return -EFAULT;
    }
    if ((_IOC_TYPE(cmd) != AESD_IOC_MAGIC) || (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR))
    {
        return -ENOTTY;
    }
    
    switch (cmd)
    {
      case AESDCHAR_IOCSEEKTO:
        if (copy_from_user(&seeker, (const void __user *)arg, sizeof(seeker)) == 0)  //copy data from the user space into the seeker variable
        {
           retval = file_offset_move(filp, seeker.write_cmd, seeker.write_cmd_offset);
        }
        else
        {
           retval = -EFAULT;
        }
        break;

      default:
 	retval = -ENOTTY;
 	break;
     }
  return retval;
}

struct file_operations aesd_fops = {
.owner =    THIS_MODULE,
.read =     aesd_read,
.write =    aesd_write,
.open =     aesd_open,
.release =  aesd_release,
.llseek = aesd_llseek,
.unlocked_ioctl = aesd_ioctl
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
    mutex_init(&aesd_device.mutex_lock); //initializes the mutex
    aesd_circular_buffer_init(&aesd_device.cbuff); //initializes a circular buffer within the aesd_device structure
    result = aesd_setup_cdev(&aesd_device);

    if( result )  //checking if an error occurred during the setup and registration of the character device
    {
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
    //deallocates memory associated with the buffer pointed to by aesd_device.buffer_entry.buffptr 
    //kfree(aesd_device.buffer_entry.buffptr);
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.cbuff, index)
    {
        if(entry->buffptr != NULL) //If it's not `NULL, it means that there is memory associated with this entry
        {
            kfree(entry->buffptr); //deallocating memory associated with the buffptr
        }
        mutex_destroy(&aesd_device.mutex_lock);
    }

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
