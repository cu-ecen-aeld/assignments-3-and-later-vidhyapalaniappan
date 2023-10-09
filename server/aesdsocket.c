/********************************************************************************************************************************************************
 File name        : aesdsocket.c
 ​Description      : A socket program for server in stream mode
 File​ ​Author​ ​Name : Vidhya. PL
 Date             : 10/08/2023
 Reference        : https://beej.us/guide/bgnet/html/#getaddrinfoprepare-to-launch 
                    https://www.thegeekstuff.com/2012/02/c-daemon-process/
                    Binding function: ChatGPT at https://chat.openai.com/ with prompts including 
                    "Binding function for server using sockaddr_in"
 **********************************************************************************************************************************************************
*/

/*Including necessary header files*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/types.h>

/*Defining MACROS*/
#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024
#define MAX_BACKLOG 5
#define MAX_IP_LEN INET_ADDRSTRLEN

/*Defining ERROR codes*/
#define ERROR_SOCKET_CREATE  -1
#define ERROR_SOCKET_OPTIONS -2
#define ERROR_SOCKET_BIND    -3
#define ERROR_LISTEN         -4
#define ERROR_FILE_OPEN      -5
#define ERROR_MEMORY_ALLOC   -6

// File Permissions
#define FILE_PERMISSIONS 0644

int sockfd;
int client_fd;


/* Function prototypes */
static void signal_handler(int signo);
static int daemon_func();
static void exit_func();
static int setup_socket();
static void handle_client_connection(int client_fd);


/*----------------------------------------------------------------------------
 int main(int argc, char **argv)
 *@brief : Main function
 *
 *Parameters : argc, argv
 *
 *Return : 0 on completion
 *
 *----------------------------------------------------------------------------*/
int main(int argc, char **argv) 
{
    openlog(NULL, 0, LOG_USER);
    syslog(LOG_INFO, "Starting aesdsocket");

    int daemon_set = 0; //Variable used to determine whether the program should run as a daemon.

    //checking if -d argument is given for daemon. If given, setting the daemon flag
    if (argc > 1 && strcmp(argv[1], "-d") == 0) 
    {
        daemon_set = 1;
    } 
    else 
    {
        syslog(LOG_ERR, "-d is not passed for daemon");
    }

    //If daemon flag is set, call the daemon function
    if (daemon_set) 
    {
        if (daemon_func() == -1) 
        {
            syslog(LOG_ERR, "Failed to create Daemon");
        } else 
        {
            syslog(LOG_DEBUG, "Successfully created Daemon");
        }
    }

    //Checking for SIGTERM and SIGINT signals and calling the signal handler function
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    sockfd = setup_socket();  //Creating a socket
    if (sockfd == -1) {
        exit_func();
        return ERROR_SOCKET_CREATE;
    }

    syslog(LOG_INFO, "Server is listening on port %d", PORT);

    while (1) 
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);  //continuously accepting incoming client connections 
        if (client_fd == -1) {
            perror("accept");
            syslog(LOG_ERR, "Error accepting client connection: %m");
            continue;
        }

        handle_client_connection(client_fd);
    }

    exit_func(); //perform cleanup tasks
    return 0;
}


/*----------------------------------------------------------------------------
static void signal_handler(int signo) 
 *@brief : Function to handle the incomming signals
 *
 *Parameters : signal number that triggered the signal handler
 *
 *Return : none
 *
 *----------------------------------------------------------------------------*/
static void signal_handler(int signo) 
{
    if (signo == SIGINT || signo == SIGTERM)           //Checking Received signal is SIGINT or SIGTERM
    {
        syslog(LOG_INFO, "Caught signal, exiting");
        if (unlink(DATA_FILE) == -1)                   // Delete file path
        { 
            syslog(LOG_ERR, "Error removing data file: %m");
        }
    	if(close(sockfd) == -1)
	{
    	     syslog(LOG_ERR, "Error closing data file");
        }
       if(close(client_fd) == -1)
        {
    	     syslog(LOG_ERR, "Error closing client fd");
        }
        closelog();
        exit(1);
    }
}


/*----------------------------------------------------------------------------
static int daemon_func() 
 *@brief : Function to create a daemon process
 *
 *Parameters : none
 *
 *Return : 0
 *
 *----------------------------------------------------------------------------*/
static int daemon_func() 
{
    pid_t pid = fork();   //creating a child process by calling the fork function.
    if (pid < 1) 
    {
        perror("fork");
        syslog(LOG_ERR, "Error forking: %m");
        return -1;
    } 
    else if (pid > 0) 
    {
        exit(EXIT_SUCCESS);
    }
    
    if (setsid() < 0) //creating a new session for the child process using the setsid function.
    {
        perror("setsid"); 
        syslog(LOG_ERR, "Error creating daemon session: %m");
        return -1;
    }

    if (chdir("/") == -1)  //changing the current working directory of the child process to the root directory 
    {
        perror("chdir");
        syslog(LOG_ERR, "Error changing working directory: %m");
        return -1;
    }
    
    // closing the standard input (0), standard output (1), and standard error (2) file descriptors for the daemon process.
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    return 0;
}


/*----------------------------------------------------------------------------
static void exit_func()
 *@brief : Function to perform clean up operations
 *
 *Parameters : none
 *
 *Return : none
 *
 *----------------------------------------------------------------------------*/
static void exit_func() 
{
    if(close(sockfd) == -1)
    {
    	syslog(LOG_ERR, "Error closing data file");
    }
    if (unlink(DATA_FILE) == -1) {
        syslog(LOG_ERR, "Error removing data file: %m");
    }
    if(close(client_fd) == -1)
    {
    	syslog(LOG_ERR, "Error closing client fd");
    }
    closelog();
}


/*----------------------------------------------------------------------------
static int setup_socket() 
 *@brief : Function to create socket, bind and listen
 *
 *Parameters : none
 *
 *Return : sockfd
 *
 *----------------------------------------------------------------------------*/
static int setup_socket() 
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);  //creating a socket using the socket function.
    if (sockfd == -1) 
    {
        perror("socket");
        syslog(LOG_ERR, "Error creating socket: %m");
        return ERROR_SOCKET_CREATE;
    }

    int yes = 1;
    // setting a socket option called SO_REUSEADDR on the socket to allow reuse of the socket address
    // useful for cases where the server needs to restart and bind to the same port quickly
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) 
    {
        perror("setsockopt");
        syslog(LOG_ERR, "Error setting socket options: %m");
        close(sockfd);
        return ERROR_SOCKET_OPTIONS;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;         //IPv4
    server_addr.sin_port = htons(PORT);       //setting the port to the desired port number
    server_addr.sin_addr.s_addr = INADDR_ANY; //binds the socket to any available network interface 

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)     //binding the socket to the specified address and port using the bind function
    {
        perror("bind");
        syslog(LOG_ERR, "Error binding socket: %m");
        close(sockfd);
        return ERROR_SOCKET_BIND;
    }

    if (listen(sockfd, MAX_BACKLOG) == -1)    //setting the socket to the listening state using the listen function
    {
        perror("listen");
        syslog(LOG_ERR, "Error listening on socket: %m");
        close(sockfd);
        return ERROR_LISTEN;
    }

    return sockfd;
}


/*----------------------------------------------------------------------------
static void handle_client_connection(int client_fd) 
 *@brief : Function to receive and send data from and to the client
 *
 *Parameters : client_fd
 *
 *Return : none
 *
 *----------------------------------------------------------------------------*/
static void handle_client_connection(int client_fd) 
{
    char client_ip[MAX_IP_LEN];                   //to store information about the client's IP address. 
    struct sockaddr_in client_addr;               //structure to hold client socket address information
    socklen_t client_len = sizeof(client_addr);

    getpeername(client_fd, (struct sockaddr *)&client_addr, &client_len);  //retrieve the IP address of the connected client
    
    //extracting the client's IP address from the client_addr structure and converting it from binary form to a string using inet_ntop.
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, MAX_IP_LEN);

    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    char *buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);   //buffer used to read and write data to and from the client
    if (buffer == NULL) 
    {
        syslog(LOG_ERR, "Could not allocate memory");
        exit(ERROR_MEMORY_ALLOC);
    } else 
    {
        syslog(LOG_ERR, "Successfully created buffer\n");
    }

    //opening a data file (DATA_FILE) in write-only mode with options to create the file if it doesn't exist and to append data to it.
    int data_fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, FILE_PERMISSIONS);
    if (data_fd == -1) 
    {
        perror("open");
        syslog(LOG_ERR, "Error opening data file: %m");
        free(buffer);
        close(client_fd);
        return;
    }

    ssize_t bytes_received;

    //reading data from the client socket in chunks of up to BUFFER_SIZE bytes using the recv function.
    while ((bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0)  
    {
        write(data_fd, buffer, bytes_received);           //writing the received data to the data file using the write function.
        if (memchr(buffer, '\n', bytes_received) != NULL) //checking if the received data contains a newline character 
        {
            break;
        }
    }

    syslog(LOG_INFO, "Received data from %s: %s, %d", client_ip, buffer, (int)bytes_received);

    close(data_fd);

    lseek(data_fd, 0, SEEK_SET);  //line seeks to the beginning of the data file using lseek

    data_fd = open(DATA_FILE, O_RDONLY);
    if (data_fd == -1) {
        perror("open");
        syslog(LOG_ERR, "Error opening data file for reading: %m");
        free(buffer);
        close(client_fd);
        return;
    }

    //to store the number of bytes read from the data file and resetting the buffer to all zeros.
    ssize_t bytes_read;
    memset(buffer, 0, sizeof(char) * BUFFER_SIZE);
    
    //reading data from the data file into the buffer in chunks of up to BUFFER_SIZE bytes using the read function
    while ((bytes_read = read(data_fd, buffer, sizeof(char) * BUFFER_SIZE)) > 0)  
    {
        send(client_fd, buffer, bytes_read, 0);  //sending the data read from the file back to the client 
    }

    free(buffer);
    
    if(close(data_fd) == -1)
    {
    	syslog(LOG_ERR, "Error closing data file");
    }
    if(close(client_fd) == -1)
    {
    	syslog(LOG_ERR, "Error closing client fd");
    }

    syslog(LOG_INFO, "Closed connection from %s", client_ip);
}

