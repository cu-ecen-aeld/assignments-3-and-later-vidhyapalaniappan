/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 * @modified by : Vidhya.PL
 * @reference : ChatGPT - https://chat.openai.com/
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
    
  // checking whether the buffer pointer and entry_offset_byte_rtn pointer are null pointers   
  if (!buffer || !entry_offset_byte_rtn) 
  {
        return NULL;
  }

   size_t sum_offset = 0; //variable is used to keep track of the cumulative character offset within a circular buffer as the function 
   int given_offset = buffer->out_offs;  //to keep track of the index of the buffer entry being currently examined in the circular buffer

   for (int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)  //to iterate over buffer entries
   {
       if (char_offset < sum_offset + buffer->entry[given_offset].size) //checking if the char_offset falls within the range of characters covered by the current buffer entry.
       {
           if (entry_offset_byte_rtn) //checking if entry_offset_byte_rtn is not a null pointer
           {
            
               *entry_offset_byte_rtn = char_offset - sum_offset;  // Calculate the byte offset within the entry.
           }
           return &buffer->entry[given_offset];  // Return the corresponding buffer entry.
       }
        
       sum_offset += buffer->entry[given_offset].size; //updating the sum_offset as the function moves from one entry to the next
       given_offset = (given_offset + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; //to ensure that the function iterates through all available buffer entries and the circular buffer structure is properly maintained
    }
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
char *aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
    
    char *return_value = NULL;    
    //checking if the pointer buffer and the pointer add_entry are null pointers    
    if (!buffer || !add_entry) 
    {
        return return_value;
    }

    buffer->entry[buffer->in_offs] = *add_entry;  //adding the add_entry to the circular buffer at the current insertion position.

    if (buffer->full) //If full, out_offs counter which is used to keep track of the position where entries are being removed is incremented
    {
    	return_value = (char *)buffer->entry[buffer->out_offs].buffptr;
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;  //incrementing the in_offs counter to prepare for the next insertion

    if ((buffer->in_offs == buffer->out_offs) && (!(buffer->full))) //checking if circular buffer is now full
    {
        buffer->full = true;
    }
    return return_value;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
