/********************************************************************************************************************************************************
 File name: writer.c
 ​Description: To write a string to a file
 File​ ​Author​ ​Name: Vidhya. PL
 Date : 09/10/2023
 Reference : file operations: https://www.guru99.com/c-file-input-output.html#:~:text=To%20create%20a%20file%20in,used%20to%20open%20a%20file.
 	     Syslog syntax : ChatGPT at https://chat.openai.com/ with prompts including 
 	     "Use the syslog capability to write a message “Writing <string> to <file>” where <string> is the text string
 	     written to file (second argument) and <file> is the file created by the script. "
 **********************************************************************************************************************************************************
 */
 
 
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>


/*
 * int main(int argc, char *argv[])
 *
 * @brief   This function is the main entry point for the program. It is used to write a string to a file.
 *
 * @param   argc  The number of command-line arguments.
 * @param   argv  An array of strings representing the command-line arguments.
 *
 * @return  Returns an integer, typically 0 on successful execution, and 1 on error.
 */

int main(int argc, char *argv[])
{
    openlog(NULL, 0, LOG_USER);   // Initializing syslog
     
    if (argc != 3)   //Checking if the correct number of command-line arguments is provided
    {
        fprintf(stderr, "Usage: %s <file_path> <write_string>\n", argv[0]);
        syslog(LOG_ERR, "Error : Exactly two arguments are not passed \n");
        syslog(LOG_ERR, "Usage : %s <file_path> <write_string>\n", argv[0]);
        return 1;   //Exiting the program with an error code
    }


    char *file_name = argv[1];  //Extracting and storing the file path and write string from command-line arguments
    char *write_str = argv[2];


    FILE *fp;

    fp = fopen(file_name, "w+"); // Opening the file in write mode
    
    if (fp == NULL) 
    {
	fprintf(stderr,"File open was unsucessful");
	syslog(LOG_ERR, "Error opening the specified file");
        return 1;
    }


    if (fputs(write_str, fp) != EOF)  // Writing the specified string to the file
    {
    	syslog(LOG_DEBUG, "Writing %s to %s", write_str, file_name);
    	syslog(LOG_DEBUG, "Successfully written to file");
    } 
    else 
    {
    	syslog(LOG_ERR, "Error writing to file");
    } 


    if (fclose(fp) == 0) //Closing the file
    {
    	syslog(LOG_DEBUG, "File closed successfully");
    }
    else 
    {
    	syslog(LOG_ERR, "Error closing the file");
    }
    return 0;
}
