/********************************************************************************************************************************************************
 File name        : aesdsocket.c
 ​Description      : A socket program for server in stream mode which accepts mulitple threads and runs a seperate thread for timer
 File​ ​Author​ ​Name : Vidhya. PL
 Date             : 10/15/2023
 Reference        : Linked list : https://github.com/cu-ecen-aeld/assignments-3-and-later-tanmay-mk/blob/f3ee5ef703388693a9394793a1664ae81fb7c5d7/server/aesdsocket.c
                    Timer logic : https://github.com/cu-ecen-aeld/assignments-3-and-later-dash6424/blob/4fa53bfa3ab2770d4fc3e3fdd153001abcd2b5f3/server/aesdsocket.c
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
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <pthread.h>
#include <sys/queue.h>
#include <sys/time.h>

/*Defining MACROS*/
#define PORT 9000
#define BUFFER_SIZE 1024
#define MAX_BACKLOG 15
#define MAX_IP_LEN INET_ADDRSTRLEN
#define PERIOD 10

/*Defining ERROR codes*/
#define ERROR_SOCKET_CREATE      -1
#define ERROR_SOCKET_OPTIONS     -2
#define ERROR_SOCKET_BIND        -3
#define ERROR_LISTEN             -4
#define ERROR_FILE_OPEN          -5
#define ERROR_MEMORY_ALLOC       -6
#define MUTEX_LOCK_FAILED        -7
#define WRITE_FAILED             -8
#define LSEEK_FAILED             -9
#define SEND_FAILED             -10
#define MUTEX_UNLOCK_FAILED     -11
#define CLOCK_GETTIME_FAILED    -12
#define CLOCK_NANOSLEEP_FAILED  -13
#define TIME_FAILED             -14
#define LOCALTIME_FAILED        -15

#define USE_AESD_CHAR_DEVICE (1)

#if (USE_AESD_CHAR_DEVICE == 1)
	#define DATA_FILE "/dev/aesdchar"
#else 
	#define DATA_FILE "/var/tmp/aesdsocketdata"
#endif

/*Global variables*/
int execution_flag = 0;
int sock_fd;
int client_fd;
int data_fd;

/* Function prototypes */
static void signal_handler(int signo);
static int daemon_func();
static int setup_socket();
static void exit_func();
void *multi_thread_handler(void *arg);
void *func_timer(void *arg);

/*Structure for thread parameters*/
typedef struct
{
    pthread_t tid; //Thread id
    int thread_complete;   //flag to indicate completion of thread
    int client_fd;  //Stores the client socket descriptor.
    int data_fd;   //Stores a file descriptor of destination file
    struct sockaddr_in *client_addr; //client address
    
    #if (USE_AESD_CHAR_DEVICE == 0)
    pthread_mutex_t *mutex;  //for thread mutex
    #endif
    
}thread_parameters_t;

/*Structure for Linked List*/
typedef struct slist_data_s
{
	thread_parameters_t	thread_data;
	SLIST_ENTRY(slist_data_s) entries;
}slist_data_t;

pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;  //Declaring a mutex lock
SLIST_HEAD(slisthead, slist_data_s) head;   //Defining a Singly linked list
slist_data_t *node = NULL; 

#if (USE_AESD_CHAR_DEVICE == 0)
/*Structure for Timer node*/
typedef struct
{
    pthread_t tid;
    int data_fd;
    pthread_mutex_t *mutex;
    struct timespec *time_spec;
    time_t *time_now;
    struct tm *details_time;
}timer_struct_t;
#endif

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
    
    struct sigaction action; //structure is used to set up signal handling.

    action.sa_flags = 0;  //no special flags are set for signal handling
    action.sa_handler = &signal_handler; //setting the signal mask for the action structure using sigfillset;  all signals will be blocked to this process.

    sigfillset(&action.sa_mask);
   
    int sig_int_status = sigaction(SIGINT, &action, NULL);
    if(sig_int_status == -1 )
    {
        perror("SIGINT failed");
        closelog();
        exit(-1);
    }

    int sig_term_status = sigaction(SIGTERM, &action, NULL);
    if(sig_term_status == -1 )
    {
        perror("SIGTERM failed");
        closelog();
        exit(-1);
    }

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
        } 
        else 
        {
            syslog(LOG_DEBUG, "Successfully created Daemon");
        }
    }

    int sock_fd = setup_socket();  //Creating a socket
    if (sock_fd == -1) 
    {
        exit_func();
        return ERROR_SOCKET_CREATE;
    }
    syslog(LOG_INFO, "Server is listening on port %d", PORT);

    //Opening file with permisiions 0644 gave bad file descriptor error. So changed the arguments
    //opening a data file (DATA_FILE) for both reading and writing
    data_fd = open(DATA_FILE, (O_CREAT | O_TRUNC | O_RDWR), (S_IRWXU | S_IRWXG | S_IROTH));
    if(data_fd == -1)
    {
        closelog();
        perror("Error opening socket file:");
        return -1;
    }

#if (USE_AESD_CHAR_DEVICE == 0)
    pthread_mutex_t mutex; //initializing the mutex
    pthread_mutex_init(&mutex, NULL);

    //Populating the timer node structure with corresponding data
    timer_struct_t timer_struct;
    struct timespec time_spec;
    time_t time_now;
    struct tm details_time;
    timer_struct.mutex = &mutex;
    timer_struct.data_fd = data_fd;
    timer_struct.time_spec = &time_spec;
    timer_struct.time_now = &time_now; 
    timer_struct.details_time = &details_time;

    //Creating a pthread
    if(0 != pthread_create(&(timer_struct.tid), NULL, func_timer, &timer_struct))
    {
        /* Still continue if failure is seen */
        perror("pthread create failed: ");
    }
    else
    {
        syslog(LOG_DEBUG, "Thread create successful. thread = %ld", timer_struct.tid);
    }
#endif

    struct sockaddr_storage new_addr;
    SLIST_INIT(&head); //Linking the head of linked list
   
    while(!execution_flag)
    {
        struct sockaddr_in client_addr; //to store information about a client's network address.
        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &client_len);  //continuously accepting incoming client connections 
        if (client_fd == -1) 
        {
            perror("accept");
            syslog(LOG_ERR, "Error accepting client connection: %m");
            continue;
        }

        node = (slist_data_t *) malloc(sizeof(slist_data_t)); //to  manage information about each client connection
		SLIST_INSERT_HEAD(&head,node,entries); // inserting the node structure into a singly-linked list at the head of the list

		node->thread_data.client_fd = client_fd;
		node->thread_data.thread_complete = 0;
	#if (USE_AESD_CHAR_DEVICE == 0)	
		node->thread_data.mutex = &mutex;
	#endif	
        node->thread_data.data_fd = data_fd;
        node->thread_data.client_addr = (struct sockaddr_in *)&new_addr;

		int p_create = pthread_create(&(node->thread_data.tid),  //stores the thread's identifier.	
							NULL,			                     //default thread attributes should be used
							multi_thread_handler,		         //Function to be executed	
							&node->thread_data                   
							);

		if (p_create != 0)
		{
			syslog(LOG_ERR,"Failed to create pthread\n");
			return -1;
		}

		syslog(LOG_INFO,"Successfully created thread\n");

		SLIST_FOREACH(node,&head,entries)   // iterates through each node in the singly-linked list
		{
			pthread_join(node->thread_data.tid,NULL);   // to wait for the thread associated with the current node to finish its execution
			if(node->thread_data.thread_complete == 1)
			{
				SLIST_REMOVE(&head, node, slist_data_s, entries); // removing the current node from the singly-linked list head
				free(node);
                node = NULL;
				break;
			}
		}

    }
#if (USE_AESD_CHAR_DEVICE == 0)
    if (unlink(DATA_FILE) == -1)
    {
        perror("unlink failed: ");
    }
#endif
    if(-1 == close(sock_fd))
    {
        perror("close sock_fd failed: ");
    }

    if (close(data_fd) == -1)
    {
        perror("close fd failed: ");
    }

#if (USE_AESD_CHAR_DEVICE == 0)
    pthread_join(timer_struct.tid, NULL);  //Joining timer structures
    pthread_mutex_destroy(&mutex); //Destroying the pthread
#endif

    closelog(); 
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
    if((signo == SIGINT) || signo == SIGTERM)
    {
        execution_flag = 1;  //if a signal is recieved, only setting a flag
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
    if(close(sock_fd) == -1)
    {
    	syslog(LOG_ERR, "Error closing data file");
    }
#if (USE_AESD_CHAR_DEVICE == 0)
    if (unlink(DATA_FILE) == -1) {
        syslog(LOG_ERR, "Error removing data file: %m");
    }
#endif
    if(close(data_fd) == -1)
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
void *multi_thread_handler(void *arg)
 *@brief : Function to process and respond to multiple threads
 *
 *Parameters : arg
 *
 *Return : none
 *
 *----------------------------------------------------------------------------*/
void *multi_thread_handler(void *arg)
{
    char *recv_buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);   //buffer used to recieve data from the client
    if (recv_buffer == NULL) 
    {
        syslog(LOG_ERR, "Could not allocate memory for receive buffer");
        exit(ERROR_MEMORY_ALLOC);
    } 
    else 
    {
        syslog(LOG_ERR, "Successfully created recieve buffer\n");
    }
    memset(recv_buffer, 0, BUFFER_SIZE);
    ssize_t bytes_received;


    char *send_buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);   //buffer used to send data back
    if (send_buffer == NULL) 
    {
        syslog(LOG_ERR, "Could not allocate memory for send buffer");
        exit(ERROR_MEMORY_ALLOC);
    } 
    else 
    {
        syslog(LOG_ERR, "Successfully created send buffer\n");
    }   
    memset(send_buffer, 0, BUFFER_SIZE);
    ssize_t bytes_read; //to store the number of bytes read from the data file and resetting the buffer to all zeros.

    thread_parameters_t *thread_data = (thread_parameters_t*)arg;

    char client_ip[MAX_IP_LEN];                   //to store information about the client's IP address. 
    struct sockaddr_in client_addr;               //structure to hold client socket address information
    socklen_t client_len = sizeof(client_addr);

    getpeername(client_fd, (struct sockaddr *)&client_addr, &client_len);  //retrieve the IP address of the connected client
    
    //extracting the client's IP address from the client_addr structure and converting it from binary form to a string using inet_ntop.
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, MAX_IP_LEN);

    syslog(LOG_INFO, "Accepted connection from %s", client_ip);
    
    //Locking thread mutex
#if (USE_AESD_CHAR_DEVICE == 0)
    if (pthread_mutex_lock(thread_data->mutex) == -1)
    {
        syslog(LOG_ERR,"Failed to lock mutex");
        exit(MUTEX_LOCK_FAILED);
    }
#endif    

    //reading data from the client socket in chunks of up to BUFFER_SIZE bytes using the recv function.
    while((bytes_received = recv(thread_data->client_fd, recv_buffer, BUFFER_SIZE, 0))>0)
    {
        if (write(data_fd, recv_buffer, bytes_received) == -1)  //writing the received data to the data file using the write function.
        {
            syslog(LOG_ERR,"Failed to write to file");
            exit(WRITE_FAILED);
        }
        if (memchr(recv_buffer, '\n', bytes_received) != NULL) //checking if the received data contains a newline character  
        {
            break;
        }
    }
//#if (USE_AESD_CHAR_DEVICE == 0)
//    if (lseek(data_fd, 0, SEEK_SET) == -1)
//    {
//        syslog(LOG_ERR,"Failed: lseek");
//       exit(LSEEK_FAILED);
//    }
//#endif

    //reading data from the data file into the buffer in chunks of up to BUFFER_SIZE bytes using the read function
    while((bytes_read = read(data_fd, send_buffer, BUFFER_SIZE) )> 0)
    {
        if (send(thread_data->client_fd, send_buffer, bytes_read, 0) == -1) //sending the data read from the file back to the client 
        {
            syslog(LOG_ERR,"Failed to send data");
            exit(SEND_FAILED);
        }
    }

#if (USE_AESD_CHAR_DEVICE == 0)
    if (pthread_mutex_unlock(thread_data->mutex) == -1)   //unlocking the mutex back
    {
        syslog(LOG_ERR,"Failed to unlock mutex\n");
        exit(MUTEX_UNLOCK_FAILED);
    }
#endif
    close(thread_data->client_fd);
    thread_data->thread_complete = 1;
    free(recv_buffer);
    free(send_buffer);
    return arg;
}

/*----------------------------------------------------------------------------
void *func_timer(void *arg)
 *@brief : Thread for timer function
 *Parameters : arg
 *
 *Return : none
 *
 *----------------------------------------------------------------------------*/
#if (USE_AESD_CHAR_DEVICE == 0)
void *func_timer(void *arg)
{
    timer_struct_t *thread_data = (timer_struct_t *)arg;

    while (!execution_flag)
    {
        char buffer[BUFFER_SIZE] = {0};
        if (0 != clock_gettime(CLOCK_MONOTONIC, thread_data->time_spec))  //obtain the current time.
        {
            syslog(LOG_ERR, "Failed to get clock time");
            exit(CLOCK_GETTIME_FAILED);
        }
        thread_data->time_spec->tv_sec += PERIOD;

        if (0 != clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, thread_data->time_spec, NULL)) //used to make the thread sleep until a specific time.
        {
            syslog(LOG_ERR, "File to sleep for nanoseconds");
            exit(CLOCK_NANOSLEEP_FAILED);
            break;
        }

        if (execution_flag)
        {
            pthread_exit(NULL);
        }

        *(thread_data->time_now) = time(NULL); //retrieving the epoch time
        if (*(thread_data->time_now) == ((time_t)-1))
        {
            syslog(LOG_ERR, "Failed to retrieve epoch time");
            exit(TIME_FAILED);
        }

        thread_data->details_time = localtime(thread_data->time_now);  //conversion from the time value to the broken-down time structure
        if (thread_data->details_time == NULL)       
        {
            syslog(LOG_ERR, "Failed to get local time");
            exit(LOCALTIME_FAILED);
        }

        //function to format the broken-down time and store it in the buffer.
        int run_time = strftime(buffer, BUFFER_SIZE, "Custom timestamp: %Y, %b, %d, %H:%M:%S\n", thread_data->details_time);
        if (run_time == 0)
        {
            syslog(LOG_ERR, "Failed to format time");
        }

        if (0 != pthread_mutex_lock(thread_data->mutex))   //Locking a mutex
        {
            syslog(LOG_ERR, "Failed to lock mutex");
            exit(MUTEX_LOCK_FAILED);
        }

        if (-1 == write(thread_data->data_fd, buffer, run_time))  //writing the formatted timestamp to a file descripto
        {
            syslog(LOG_ERR, "Failed to write timestamp");
            exit(WRITE_FAILED);
        }

        if (pthread_mutex_unlock(thread_data->mutex) != 0)  //Unlocking the mutex
        {
            syslog(LOG_ERR, "Failed to unlock mutex");
            exit(MUTEX_UNLOCK_FAILED);
        }
    }
    return NULL;
}
#endif

