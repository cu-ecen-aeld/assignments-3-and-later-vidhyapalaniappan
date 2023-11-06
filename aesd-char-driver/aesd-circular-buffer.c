/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 * @modified by : Vidhya.PL
 * @reference and credits: aesd_circular_buffer_add_entry function - https://github.com/cu-ecen-aeld/assignments-3-and-later-vishalraj3112/blob/main/aesd-char-driver/aesd-circular-buffer.c
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
   
  size_t sum_offset = 0; //variable is used to keep track of the cumulative character offset within a circular buffer as the function 
  int given_offset = buffer->out_offs;  //to keep track of the index of the buffer entry being currently examined in the circular buffer
  int i;
   
  // checking whether the buffer pointer and entry_offset_byte_rtn pointer are null pointers   
  if (!buffer || !entry_offset_byte_rtn) 
  {
        return NULL;
  }
   
   for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)  //to iterate over buffer entries
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

// Check if the circular buffer is full
bool is_buffer_full(const struct aesd_circular_buffer *buffer) 
{
    return (buffer->in_offs == buffer->out_offs) && buffer->full;
}

// Handle rollover of the in_offs counter
void handle_rollover(struct aesd_circular_buffer *buffer) 
{
    if (buffer->in_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        buffer->in_offs = 0;
}

// Update the circular buffer when it's not full
void update_buffer_not_full(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry) 
{
    buffer->entry[buffer->in_offs] = *add_entry;
    buffer->in_offs++;
    handle_rollover(buffer);

    if (buffer->in_offs == buffer->out_offs)
        buffer->full = true;
    else
        buffer->full = false;
}

// Update the circular buffer when it's full and return the old value
const char *update_buffer_full(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry) {
    const char *return_value = (char *)buffer->entry[buffer->in_offs].buffptr;
    buffer->entry[buffer->in_offs] = *add_entry;
    buffer->in_offs++;
    handle_rollover(buffer);
    buffer->out_offs = buffer->in_offs;
    return return_value;
}

/*********The initial logic did not account for rollover condition. So modified the aesd_circular_buffer_add_entry function*********/
/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
struct aesd_buffer_entry *aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
        
    struct aesd_buffer_entry *retptr = NULL;

    if ( (buffer->in_offs == buffer->out_offs) && buffer->full ) 
    {
        // store value of buffptr before override 
        retptr = &(buffer->entry[buffer->in_offs]);
        // write to buffer and increment 
        buffer->entry[buffer->in_offs] = *add_entry;

        buffer->in_offs++;

        //Check rollover
        if(buffer->in_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
            buffer->in_offs = 0;

        buffer->out_offs = buffer->in_offs; 
    }
    else
    {
        buffer->entry[buffer->in_offs] = *add_entry;
        buffer->in_offs++;

        //Check rollover
        if(buffer->in_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        buffer->in_offs = 0;

        //Check for full
        if(buffer->in_offs == buffer->out_offs)
            buffer->full = true;
        else
            buffer->full = false;
        
    }

    return retptr;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
