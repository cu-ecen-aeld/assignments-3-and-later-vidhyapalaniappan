/********************************************************************************************************************************************************
 File name: systemcalls.c
 
 File​ ​modified by: Vidhya. PL
 
 Date : 09/17/2023
 
 Reference : do_exec() : reference taken from ChatGPT at https://chat.openai.com/ with prompts including 
 	     Syslog syntax : ChatGPT at https://chat.openai.com/ with prompts including 
 	     "Complete the TODO for do_exec() function"
 	     do_exec_redirect() : https://stackoverflow.com/a/13784315/1446624
 ********************************************************************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>

#include "systemcalls.h"
/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    openlog("do_system function", 0, LOG_USER);
    
    int sys_return = system(cmd);    //Calling the system() function with command set in cmd
    
    if(sys_return == 0 )             //Checking for return value of system function.
    {
        syslog(LOG_DEBUG, "Successfully completed system(cmd)");
    	return true;
    }
    
    else
    {
    	return false;
    	syslog(LOG_ERR, "Error : system(cmd) failed \n");
    }
    closelog();
    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, fado_execlse if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    openlog("do_exec function", 0, LOG_USER);
    
    pid_t pid = fork();    //calling the fork function
    
    if (pid == -1)         //checking for return value of fork()
    {
     	syslog(LOG_ERR, "Error : fork() failed \n");
        perror("fork failed");
        va_end(args);
        closelog();
        return false;
    } 
    
    else if (pid == 0) // Child process is created
    { 
        syslog(LOG_DEBUG, "Successfully created child process");
        execv(command[0], command);
    	perror("execvp"); 
   	syslog(LOG_ERR, "Error : execvp failed \n");
        closelog();
        exit(EXIT_FAILURE);
    } 
    
    else // Parent process
    { 
    	syslog(LOG_DEBUG, "Parent process is running");
        int status;
        waitpid(pid, &status, 0); 
        
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) 
        {
            syslog(LOG_DEBUG, "Successfully completion of wait");
            va_end(args);
            closelog();
            return true;
         } 
         
         else 
         {
            syslog(LOG_ERR, "Error : wait failed \n");
            va_end(args);
            closelog();
            return false;
         }
    }    
    va_end(args);
    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removedhttps://stackoverflow.com/a/13784315/1446624
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    openlog("do_exec_redirect function", 0, LOG_USER);
    
    int outputfile_fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (outputfile_fd == -1) 
    {
        perror("Opening output file failed");
        syslog(LOG_ERR, "Error : failed to open output fail \n");
        va_end(args);
        closelog();
        return false;
    }
    
    pid_t pid = fork();  //creating a fork
    
    if (pid == -1) 
    {
        syslog(LOG_ERR, "Error : fork() failed \n");
        perror("Fork failed");
        va_end(args);
        closelog();
        return false;
    }
    
    else if (pid == 0) // Child process
    {  
    	if (dup2(outputfile_fd, 1) < 0)     // Redirect standard output to the output file
    	{ 
    	     syslog(LOG_DEBUG, "Successfully created child process");   
    	     perror("dup2"); 
    	     closelog();
    	     exit(EXIT_FAILURE);    		
    	}
    	
    close(outputfile_fd);
    execvp(command[0], command); 
    perror("execvp"); 
    syslog(LOG_ERR, "Error : execvp failed \n");
    exit(EXIT_FAILURE);   
    }
    
    else
    {
    	syslog(LOG_DEBUG, "Parent process is running");
    	int status;
        waitpid(pid, &status, 0);
        close(outputfile_fd);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) 
        {
            va_end(args);
            syslog(LOG_DEBUG, "Successfully completion of wait");
            closelog();
            return true;
        } 
        
        else 
        {
            syslog(LOG_ERR, "Error : wait failed \n");
            va_end(args);
            closelog();
            return false;
        }
    }
    
    va_end(args);
    return true;
}
