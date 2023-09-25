/********************************************************************************************************************************************************
 File name: threading.c
 Description: To create a thread that waits, obtains a mutex, holds it, and then releases it after certain time intervals 
 Modified by: Vidhya. PL
 Date : 09/20/2023
 Credits : start_thread_obtaining_mutex() was partially generated using ChatGPT at https://chat.openai.com/ 
 	   with prompts including "Complete the TODO for this functions"
 **********************************************************************************************************************************************************
 */

#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
 
    //Casting the thread parameter back to the expected struct type. 	
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    // Sleep for wait_to_obtain_ms milliseconds.
    usleep(thread_func_args->wait_to_obtain_ms * 1000);
    DEBUG_LOG("Sleeping for %d milliseconds before obtaining mutex.", thread_func_args->wait_to_obtain_ms);

    // Attempt to obtain the mutex.
    DEBUG_LOG("Attempting to obtain mutex.");
    /* From AESD lecture slide */
    if (pthread_mutex_lock(thread_func_args->mutex) != 0) {
        ERROR_LOG("Failed to obtain mutex.");
        thread_func_args->thread_complete_success = false;
        pthread_exit(NULL);
    }
     DEBUG_LOG("Successfully obtained mutex.");

    // Sleep for wait_to_release_ms milliseconds while holding the mutex.
    usleep(thread_func_args->wait_to_release_ms * 1000);
    DEBUG_LOG("Sleeping for %d milliseconds while holding mutex.", thread_func_args->wait_to_release_ms);
    
    /* From AESD lecture slide */
    if (pthread_mutex_unlock(thread_func_args->mutex) != 0) {
        ERROR_LOG("Failed to release mutex.");
        thread_func_args->thread_complete_success = false;
        pthread_exit(NULL);
    }
    DEBUG_LOG("Successfully released mutex.");

    // Marking the thread as completed successfully.
    thread_func_args->thread_complete_success = true;
    
    DEBUG_LOG("Thread completed successfully.");
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    // Allocating memory for the thread_data structure.	
    struct thread_data* thread_data_struct = (struct thread_data*)malloc(sizeof(struct thread_data));
    if (thread_data_struct == NULL) {
        ERROR_LOG("Memory allocation failed.");
        return false;
    }

    // Seting up thread_data structure with the provided parameters.
    thread_data_struct->mutex = mutex;
    thread_data_struct->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_data_struct->wait_to_release_ms = wait_to_release_ms;
    thread_data_struct->thread_complete_success = false;

    /* From AESD lecture slide */
    // Creating the thread.
    if (pthread_create(thread, NULL, threadfunc, (void*)thread_data_struct) != 0) {
        ERROR_LOG("Failed to create thread.");
        free(thread_data_struct);
        return false;
    }
   // DEBUG_LOG("Thread started successfully.");
    
    // Returning true to indicate successful thread creation.
     return true; // Thread started successfully.
}
