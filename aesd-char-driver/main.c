/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22 modified: 3/13/2022
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Vidhya. PL");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

/**
 * @desc the open call used to get the character device(cdev) from aesd_dev structure.
 * @param inode the kernel inode structure.
 * @param filp the kernel file structure passed from caller
 * @return function exit status
 */
int aesd_open(struct inode *inode, struct file *filp)
{
struct aesd_dev *dev;

PDEBUG("open");

/*store cdev in inode.ic_dev, and store in private data
 for future reference*/
dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
filp->private_data = dev;

return 0;
}

/**
 * @desc the release system call used to release andy kernel resources.
 * @param inode the kernel inode structure.
 * @param filp the kernel file structure passed from caller
 * @return function exit status
 */
int aesd_release(struct inode *inode, struct file *filp)
{
PDEBUG("release");

return 0;
}

/**
 * @desc the release system call used to release andy kernel resources.
 * @param filp the kernel file structure passed from caller.
 * @param buf the buffer pointer at which the read data needs to be stored.
 * @param count the number of bytes required to be read from kernel buffer.
 * @param f_pos the entry offset location in kernel buffer from where data will be read.
 * @return no of bytes successfully read.
 */
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
  ssize_t retval = 0;
struct aesd_dev *dev;

//entry and offset for circular buffer
struct aesd_buffer_entry *read_entry = NULL;
ssize_t read_offset = 0;
ssize_t unread_bytes = 0;
ssize_t bytes_read = 0;

PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
printk(KERN_DEBUG "read %zu bytes with offset %lld",count,*f_pos);

//get the skull device from file structure
dev = (struct aesd_dev*) filp->private_data;

//put error checks here, if count is zero, all other parameters are not null
if(filp == NULL || buf == NULL || f_pos == NULL){
return -EFAULT; //bad address
}

//lock on mutex here, preferrable interruptable, check for error
if(mutex_lock_interruptible(&dev->lock)){
PDEBUG(KERN_ERR "could not acquire mutex lock");
return -ERESTARTSYS; //check this
}

//find the read entry, and offset for given f_pos
read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->cir_buff), *f_pos, &read_offset);
if(read_entry == NULL){
goto error_exit;
}
//else{

/*check if count is greater that current max read size, then limit
 max_read_size = entry_size - read_offset
*/
if(count > (read_entry->size - read_offset))
{
bytes_read = read_entry->size - read_offset;
}
else{
bytes_read = count;
}

//}

//now read using copy_to_user
unread_bytes = copy_to_user(buf, (read_entry->buffptr + read_offset), bytes_read);

//return whatever is copied and update fpos accordingly
retval = bytes_read;
//*f_pos += retval;
*f_pos += bytes_read;

error_exit:
mutex_unlock(&(dev->lock));

return retval;
}

static ssize_t write_to_buffer(struct aesd_dev *dev, const char __user *buf, size_t count) {
    ssize_t unwritten_bytes = copy_from_user((void *)(dev->buff_entry.buffptr + dev->buff_entry.size), buf, count);
    dev->buff_entry.size += (count - unwritten_bytes);
    return (count - unwritten_bytes);
}

static int handle_circular_buffer(struct aesd_dev *dev, const char *new_entry) {
    if (new_entry) {
        kfree(new_entry);
    }
    dev->buff_entry.buffptr = NULL;
    dev->buff_entry.size = 0;
    return 0;
}

/**
 * @desc the release system call used to release andy kernel resources.
 * @param filp the kernel file structure passed from caller.
 * @param buf the buffer pointer which contains the data to be written at kernel buffer entry
 * @param count the number of bytes required to be written to kernel buffer.
 * @param f_pos the file postion location which will be updated after each write.
 * @return no of bytes successfully written.
 */
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
struct aesd_dev *dev;
const char *new_entry = NULL;
ssize_t retval = -ENOMEM;
//ssize_t unwritten_bytes = 0;
int circular_buffer_error;
PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

//cast the aesd_device from private data
dev = (struct aesd_dev*) filp->private_data;

//lock the mutex
if(mutex_lock_interruptible(&(dev->lock))){
PDEBUG(KERN_ERR "could not acquire mutex lock");
return -ERESTARTSYS;
}


if (dev->buff_entry.size == 0) {
        dev->buff_entry.buffptr = kmalloc(count * sizeof(char), GFP_KERNEL);
    } else {
        dev->buff_entry.buffptr = krealloc(dev->buff_entry.buffptr, (dev->buff_entry.size + count) * sizeof(char), GFP_KERNEL);
    }

    if (dev->buff_entry.buffptr == NULL) {
        //PDEBUG(dev->buff_entry.size == 0 ? "kmalloc error" : "krealloc error");  //fix this debug message!!
        mutex_unlock(&dev->lock);
        return -ENOMEM;
    }

retval = write_to_buffer(dev, buf, count);

    if (memchr(dev->buff_entry.buffptr, '\n', dev->buff_entry.size)) {
        new_entry = aesd_circular_buffer_add_entry(&dev->cir_buff, &dev->buff_entry);
        circular_buffer_error = handle_circular_buffer(dev, new_entry);
        if (circular_buffer_error) {
            mutex_unlock(&dev->lock);
            return circular_buffer_error;
        }
    }

PDEBUG("not doing k_free for now");

mutex_unlock(&dev->lock);

return retval;
}

struct file_operations aesd_fops = {
.owner =    THIS_MODULE,
.read =     aesd_read,
.write =    aesd_write,
.open =     aesd_open,
.release =  aesd_release,
};

/**
 * @desc this function is used to initialize the device and add it.
 * @return return value of mkdev.
 */
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

/**
 * @desc this function is used to register the device and initialize the kernel
 * data structures.
 * @return the return value of register and init functions.
 */
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

//Initialize the mutex and circular buffer
mutex_init(&aesd_device.lock);
aesd_circular_buffer_init(&aesd_device.cir_buff);

result = aesd_setup_cdev(&aesd_device);

if( result ) {
unregister_chrdev_region(dev, 1);
}
return result;

}

/**
 * @desc this function is used to unregister the device and deallocated all the kernel data structures and delte the device
 * @return none.
 */
void aesd_cleanup_module(void)
{
//free circular buffer entries
struct aesd_buffer_entry *entry = NULL;
uint8_t index = 0;

dev_t devno = MKDEV(aesd_major, aesd_minor);

cdev_del(&aesd_device.cdev);

//free the buff_entry buffpte
kfree(aesd_device.buff_entry.buffptr);

AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.cir_buff, index){
if(entry->buffptr != NULL){
kfree(entry->buffptr);
}
}

unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
