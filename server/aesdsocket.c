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
#include <pthread.h>

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

typedef struct
{
    pthread_t thread_id;
    int complete_flag;
    int client_fd;
    int fd;
    struct sockaddr_in *client_addr;
    pthread_mutex_t *mutex;
    
}thread_data_t;

typedef struct node
{
    thread_data_t thread_data;
    struct node *next;
} node_t;

pthread_mutex_t write_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t read_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Function prototypes */
static void signal_handler(int signo);
static int daemon_func();
static void exit_func();
static int setup_socket();
static void* thread_function(void* arg);
void get_rfc2822_time(char *timestamp, size_t timestamp_size);
void append_timestamp_to_file(int fd);


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
    printf("Starting aesdsocket\n\r");

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
            printf("Successfully created Daemon\n\r");
        }
    }

    //Checking for SIGTERM and SIGINT signals and calling the signal handler function
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    sockfd = setup_socket();  //Creating a socket
    if (sockfd == -1) {
        printf("Socket setup successful\n\r");
        exit_func();
        return ERROR_SOCKET_CREATE;
    }
    
    else{
    	printf("Socket setup failed\n\r");
    }

    syslog(LOG_INFO, "Server is listening on port %d", PORT);
    printf("Server id listening on port %d\n\r", PORT);

    
    //head node
    node_t *head = NULL; 
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int threadIDs = 0; 

    while (1) 
    {

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);  //continuously accepting incoming client connections 
        if (client_fd == -1) {
            perror("accept");
            syslog(LOG_ERR, "Error accepting client connection: %m");
            printf("Error accepting client connection: %m\n\r");
            continue;
        }
        else{
            printf("succesfully accepted client connection: %m\n\r");
        }
        
        int file_fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, FILE_PERMISSIONS);
   	 if (file_fd == -1) 
   	 {
      		perror("open");
        	syslog(LOG_ERR, "Error opening data file: %m");
        	printf("error opening the data file \n\r");
        	return -1;
    	}
    
        threadIDs++;
        node_t *this_client_node = (node_t *)malloc(sizeof(node_t));
        if(this_client_node == NULL)
        {
            //failed to allocate memory for node
            break;
        }

        this_client_node->thread_data.client_fd = client_fd;
        this_client_node->thread_data.client_addr = &client_addr;
        this_client_node->thread_data.complete_flag = 0; 
        this_client_node->thread_data.fd = file_fd;
        this_client_node->thread_data.mutex = &mutex;
        this_client_node->thread_data.thread_id = threadIDs;

        //handle_client_connection(client_fd);

        int th_status = pthread_create(
            &(this_client_node->thread_data.thread_id),
            NULL,
            thread_function, 
            &(this_client_node->thread_data)
        );
        if (th_status == 0)
        {
            printf("Thread created succesfully\n\r");
        }
        else 
        {
            printf("Thread creation failed\n\r");
            free(this_client_node);
        }

        this_client_node->next = head; 
        head = this_client_node;
        
        close(client_fd);
        close(file_fd); 
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
        printf("Caught signal, exiting\n\r");
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
        printf("Error forking: %m\n\r");
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


void get_rfc2822_time(char *timestamp, size_t timestamp_size) {
    time_t current_time;
    struct tm *time_info;

    time(&current_time);
    time_info = localtime(&current_time);

    strftime(timestamp, timestamp_size, "%a, %d %b %Y %T %z", time_info);
}

void append_timestamp_to_file(int fd) {
    char timestamp[64];  // Sufficient buffer for RFC 2822 timestamp
    get_rfc2822_time(timestamp, sizeof(timestamp));

    pthread_mutex_lock(&write_mutex);  // Lock the write_mutex for atomic write
    dprintf(fd, "timestamp:%s\n", timestamp);
    printf("timestamp: %s\n\r", timestamp);
    pthread_mutex_unlock(&write_mutex);
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
        printf("Error creating socket: %m\n\r");
        return ERROR_SOCKET_CREATE;
    }
    
    else{
    	printf("succesfully created socket\n\r");
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
        printf("Error binding socket: %m\n\r");
        close(sockfd);
        return ERROR_SOCKET_BIND;
    }
    else{
    	printf("successfully binded\n\r");
    }

    if (listen(sockfd, MAX_BACKLOG) == -1)    //setting the socket to the listening state using the listen function
    {
        perror("listen");
        syslog(LOG_ERR, "Error listening on socket: %m");
        printf("Error listening on socket: %m\n\r");
        close(sockfd);
        return ERROR_LISTEN;
    }
    else{
    	printf("Listening on socket: %m\n\r");
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
static void* thread_function(void* arg) 
{
  thread_data_t* thread_data = (thread_data_t*)arg;
    char client_ip[MAX_IP_LEN];                   //to store information about the client's IP address. 
    struct sockaddr_in client_addr;               //structure to hold client socket address information
    socklen_t client_len = sizeof(client_addr);
    int client_fd = thread_data->client_fd;
    getpeername(client_fd, (struct sockaddr *)&client_addr, &client_len);  //retrieve the IP address of the connected client
    
    //extracting the client's IP address from the client_addr structure and converting it from binary form to a string using inet_ntop.
inet_ntop(AF_INET, &(thread_data->client_addr->sin_addr), client_ip, MAX_IP_LEN);

    syslog(LOG_INFO, "Accepted connection from %s", client_ip);
    printf("Accepted connection from %s\n\r", client_ip);
    char *buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);   //buffer used to read and write data to and from the client
    if (buffer == NULL) 
    {
        syslog(LOG_ERR, "Could not allocate buffer memory");
        printf("Could not allocate buffer memory\n\r");
        exit(ERROR_MEMORY_ALLOC);
    } else 
    {
        syslog(LOG_ERR, "Successfully created buffer\n");
        printf("Successfully created buffer\n\r");
    }

    //opening a data file (DATA_FILE) in write-only mode with options to create the file if it doesn't exist and to append data to it.
 /*   int thread_data->fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, FILE_PERMISSIONS);
    if (thread_data->fd == -1) 
    {
        perror("open");
        syslog(LOG_ERR, "Error opening data file: %m");
        free(buffer);
        close(thread_data->client_fd);
        return;
    }*/

    ssize_t bytes_received;

    //reading data from the client socket in chunks of up to BUFFER_SIZE bytes using the recv function.
    while ((bytes_received = recv(thread_data->client_fd, buffer, BUFFER_SIZE, 0)) > 0)  
    {
    	printf("In the recv while loop\n\r");
    	pthread_mutex_lock((thread_data->mutex));
        write(thread_data->fd, buffer, bytes_received);           //writing the received data to the data file using the write function.
        pthread_mutex_unlock((thread_data->mutex));
        if (time(NULL) % 10 == 0) {
            append_timestamp_to_file(thread_data->fd);
        }
        if (memchr(buffer, '\n', bytes_received) != NULL) //checking if the received data contains a newline character 
        {
            break;
        }
    }

    syslog(LOG_INFO, "Received data from %s: %s, %d", client_ip, buffer, (int)bytes_received);
    printf("Received data from %s: %s, %d\n\r", client_ip, buffer, (int)bytes_received);
    close(thread_data->fd);
    
    lseek(thread_data->fd, 0, SEEK_SET);  //line seeks to the beginning of the data file using lseek
/*
    thread_data->fd = open(DATA_FILE, O_RDONLY);
    if (thread_data->fd == -1) {
        perror("open");
        syslog(LOG_ERR, "Error opening data file for reading: %m");
        free(buffer);
        close(thread_data->client_fd);
        return;
    }*/

    //to store the number of bytes read from the data file and resetting the buffer to all zeros.
    ssize_t bytes_read;
    memset(buffer, 0, sizeof(char) * BUFFER_SIZE);
    
    //reading data from the data file into the buffer in chunks of up to BUFFER_SIZE bytes using the read function
    while ((bytes_read = read(thread_data->fd, buffer, sizeof(char) * BUFFER_SIZE)) > 0)  
    {
    	printf("In the send while loop\n\r");
    	printf("Sending data: %.*s\n", (int)bytes_read, buffer);
        send(thread_data->client_fd, buffer, bytes_read, 0);  //sending the data read from the file back to the client 
    }
    
    free(buffer);
    
    if(close(thread_data->fd) == -1)
    {
    	syslog(LOG_ERR, "Error closing data file");
    }
    if(close(thread_data->client_fd) == -1)
    {
    	syslog(LOG_ERR, "Error closing client fd");
    }

    close(client_fd);
    close(thread_data->fd);
    
    pthread_mutex_lock(thread_data->mutex);
    thread_data->complete_flag = 1;
    pthread_mutex_unlock(thread_data->mutex);
    
    syslog(LOG_INFO, "Closed connection from %s", client_ip);
    printf("Closed connection from %s\n\r", client_ip);
    return NULL;
}
