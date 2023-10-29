/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#include "aesd-circular-buffer.h"

#define AESD_DEBUG 1  //Remove comment on this line to enable debug

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#define MAX_WRITE_COMMANDS 10
#define MAX_CONTENT_SIZE 128

struct write_command_entry {
    char content[MAX_CONTENT_SIZE];
    size_t length;
};

struct aesd_dev
{
    /**
     * TODO: Add structure(s) and locks needed to complete assignment requirements
     */
    struct cdev cdev;     /* Char device structure      */
    struct aesd_circular_buffer cbuff;
    struct mutex mutex_lock;
    char *cbuff_entry;
    char cbuff_size;
    int aesd_write_commands_start; 
    int aesd_write_commands_count;    /* Count of write commands */
    struct write_command_entry aesd_write_commands[MAX_WRITE_COMMANDS];
};


#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
