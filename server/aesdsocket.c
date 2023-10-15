#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <syslog.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/fs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/queue.h>
#include <sys/time.h>

#define PORT 9000
#define BACKLOG 15
#define INIT_BUF_SIZE 1024
#define SOCKET_ADDR "/var/tmp/aesdsocketdata"
#define TIME_PERIOD 10

/*Defining ERROR codes*/
#define ERROR_SOCKET_CREATE  -1
#define ERROR_SOCKET_OPTIONS -2
#define ERROR_SOCKET_BIND    -3
#define ERROR_LISTEN         -4
#define ERROR_FILE_OPEN      -5
#define ERROR_MEMORY_ALLOC   -6

int complete_exec = 0;

int sfd;
int client_fd;

/* Function prototypes */
static void signal_handler(int signo);
static int daemon_func();
static int setup_socket();
static void exit_func();

typedef struct
{
    pthread_t thread_id;
    int complete_flag;
    int client_fd;
    int fd;
    struct sockaddr_in *client_addr;
    pthread_mutex_t *mutex;
    
}thread_data_t;

typedef struct slist_data_s
{
	thread_data_t	thread_data;
	SLIST_ENTRY(slist_data_s) entries;
}slist_data_t;

pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;
SLIST_HEAD(slisthead, slist_data_s) head;
slist_data_t 	*datap 			= NULL;


typedef struct
{
    pthread_t thread_id;
    int fd;
    pthread_mutex_t *mutex;
}timer_node_t;


static void signal_handler(int signo)
{
    if((signo == SIGINT) || signo == SIGTERM)
    {
        complete_exec = 1;
    }
}

int signal_init()
{
    struct sigaction sig_action;
    sig_action.sa_handler = &signal_handler;

    sigfillset(&sig_action.sa_mask);

    sig_action.sa_flags = 0;

    if(-1 == sigaction(SIGINT, &sig_action, NULL))
    {
        perror("sigaction failed: ");
        exit(EXIT_FAILURE);
    }

    if(-1 == sigaction(SIGTERM, &sig_action, NULL))
    {
        perror("sigaction failed: ");
        exit(EXIT_FAILURE);
    }
    return 0;
}

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

void *timer_thread(void *thread_params)
{
    if(NULL == thread_params)
    {
        return NULL;
    }

    timer_node_t *thread_data = (timer_node_t *)thread_params;
    struct timespec ts;
    time_t t;
    struct tm* temp;

    while(!complete_exec)
    {
        char buffer[INIT_BUF_SIZE] = {0};

        if(0 !=clock_gettime(CLOCK_MONOTONIC, &ts))
        {
            perror("Clock get time failed: ");
            break;
        }
        ts.tv_sec += TIME_PERIOD;

        /* If signal received exit thread */
        if(complete_exec)
        {
            pthread_exit(NULL);
        }

        if(0 != clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL))
        {
            perror("clock nanosleep failed: ");
            break;
        }
        /* If signal received exit thread */
        if(complete_exec)
        {
            pthread_exit(NULL);
        }
        
        t = time(NULL);
        if (t == ((time_t) -1))
        {
            perror("time since epoch failed: ");
            break;
        }

        temp = localtime(&t);
        if (temp == NULL)
        {
            perror("Local time failed: ");
            break;
        }

        int len = strftime(buffer, INIT_BUF_SIZE, "timestamp: %Y, %b, %d, %H:%M:%S\n", temp);
        if(len == 0)
        {
            syslog(LOG_ERR, "Failed to get time");
        }

        if (0 != pthread_mutex_lock(thread_data->mutex)) {
            perror("Mutex lock failed: ");
            break;
        }

        if (-1 == write(thread_data->fd, buffer, len))
        {
            perror("write timestamp failed: ");
        }

        if (pthread_mutex_unlock(thread_data->mutex) != 0) {
            perror("Mutex unlock failed: ");
            break;
        }
    }

    return NULL;
}

void *thread_socket(void *thread_params)
{
    if(NULL == thread_params)
    {
        return NULL;
    }
    thread_data_t *thread_data = (thread_data_t*)thread_params;

    char address_string[INET_ADDRSTRLEN];       // Holds the IP address from client
    syslog(LOG_DEBUG, "Accepted connection from %s", inet_ntop(AF_INET, &(thread_data->client_addr->sin_addr), address_string, sizeof(address_string)));
    
    /* Buffer for receiving data from client */
    char *storage_buffer = (char *)malloc(INIT_BUF_SIZE);
    if(!storage_buffer)
    {
        perror("malloc failure: ");
        goto err_handling;
    }

    /* Static buffer for receiving data from client */
    char buffer[INIT_BUF_SIZE];                 // Static buffer of 1024 bytes to receive data
    int storage_buffer_size = INIT_BUF_SIZE;    // Static buffer size

    int storage_buffer_size_cnt = 1;            // Count to indicate the number of string received from client

    int complete_flag = 0;                      // Idetentifier of string complete.


    
    int actual_bytes_rcvd = 0;
    int bytes_rcvd;                             // Stores the bytes received from recv function
    int bytes_to_write;                         // Number of bytes to be written to fd.
    ssize_t rd_bytes = 0;

    // Receive bytes by polling the client fd for every 1024 bytes
    while((bytes_rcvd = recv(thread_data->client_fd, buffer, INIT_BUF_SIZE, 0)) > 0)
    {
        for(int i=0; i < bytes_rcvd; i++)
        {
            if(buffer[i] == '\n')
            {
                complete_flag = 1;
                actual_bytes_rcvd = i+1;
                break;
            }
        }
        if(complete_flag == 1)
        {
            memcpy(storage_buffer + (storage_buffer_size_cnt - 1)*INIT_BUF_SIZE, buffer, actual_bytes_rcvd);
            bytes_to_write = (storage_buffer_size_cnt - 1)*INIT_BUF_SIZE + actual_bytes_rcvd;
            break;
        }
        else    // reallocated the buffer by multiples of 1024 until complete flag is set
        {
            memcpy(storage_buffer + (storage_buffer_size_cnt - 1)*INIT_BUF_SIZE, buffer, bytes_rcvd);
            char *temp_ptr = (char *)realloc(storage_buffer, (storage_buffer_size + INIT_BUF_SIZE));
            if(temp_ptr)
            {
                storage_buffer = temp_ptr;
                storage_buffer_size += INIT_BUF_SIZE;
                storage_buffer_size_cnt++;
            }
            else
            {
                syslog(LOG_ERR,"No memory available to alloc");
            }
        }
    }
    syslog(LOG_DEBUG, "Storage buffer size = %d", bytes_to_write);
    if(-1 == bytes_rcvd)
    {
        perror("receive failed: ");
        goto err_handling;
    }

    // If transmission is complete,
    // Copy to local fd and then rewrite/resend it back to client.
    if(1 == complete_flag)
    {
        complete_flag = 0;

        /* Malloc 1kB for reading the buffer */
        char read_buffer[INIT_BUF_SIZE] = {0};

        /* Mutex lock */
        if(0 != pthread_mutex_lock(thread_data->mutex))
        {
            perror("Mutex Lock Failed: ");
            goto err_handling;
        }

        //Write to file
        write(thread_data->fd, storage_buffer, bytes_to_write);        

        //Set the fd to start of the file.
        lseek(thread_data->fd, 0, SEEK_SET);

        while((rd_bytes = read(thread_data->fd, read_buffer, INIT_BUF_SIZE)) > 0)
        {
            if(-1 == send(thread_data->client_fd, read_buffer, rd_bytes, 0))
            {
                perror("send failed: ");
                break;
            }
        }

        /* Mutex unlock */
        if(0 != pthread_mutex_unlock(thread_data->mutex))
        {
            perror("Mutex unlock failed: ");
        }

        if(-1 == rd_bytes)
        {
            perror("read failed: ");
        }
    }
    /* Clean up code */
err_handling:
    if(storage_buffer)
    {
        free(storage_buffer);
        storage_buffer = NULL;
    }
    /* Set the complete flag to 1 */
    thread_data->complete_flag = 1;
    /* Close client fd */
    close(thread_data->client_fd);

    syslog(LOG_DEBUG, "Closed connection from %s", address_string);
    return NULL;
}

static void exit_func() 
{
    if(close(sfd) == -1)
    {
    	syslog(LOG_ERR, "Error closing data file");
    }
    if (unlink(SOCKET_ADDR) == -1) {
        syslog(LOG_ERR, "Error removing data file: %m");
    }
    // if(close(client_fd) == -1)
    // {
    // 	syslog(LOG_ERR, "Error closing client fd");
    // }
    closelog();
}

int main(int argc, char **argv)
{
    openlog(NULL, 0, LOG_USER);

    //Daemon flag if -d is passed as an argument.
    int daemon_set = 0;

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

    /* Signal init */
    if(0 != signal_init())
    {
        closelog();
        return -1;
    }

    // //Socket variables.
    // int sock_status = 0;
    // struct addrinfo hints;
    // struct addrinfo *servinfo = NULL;

    // memset(&hints, 0, sizeof(hints));   // clear the struct
    // hints.ai_family = AF_INET;          // IPv4
    // hints.ai_socktype = SOCK_STREAM;    // TCP socket
    // hints.ai_flags = AI_PASSIVE;        // Query for IP

    // // get server info
    // sock_status = getaddrinfo(NULL, "9000", &hints, &servinfo);
    // if(0 != sock_status)
    // {
    //     closelog();
    //     perror("getaddrinfo failure: ");
    //     return -1;
    // }
    // if(!servinfo)
    // {
    //     closelog();
    //     perror("servinfo struct was not populated: ");
    //     return -1;
    // }

    // // create socket and get file descriptor
    // int sfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    // if(-1 == sfd)
    // {
    //     closelog();
    //     perror("Failed to get socket fd: ");
    //     freeaddrinfo(servinfo);
    //     return -1;
    // }

    // //setup socket

    // /* Set socket to non blocking */
    // int flags = fcntl(sfd, F_GETFL, 0);
    // fcntl(sfd, F_SETFL, flags | O_NONBLOCK);

    // // reuse the socket port
    // int sock_opt = 1;
    // sock_status = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt));
    // if(-1 == sock_status)
    // {
    //     perror("setsockopt failed: ");
    //     closelog();
    //     freeaddrinfo(servinfo);
    //     return -1;
    // }

    // // bind the socket to port
    // sock_status = bind(sfd, servinfo->ai_addr, sizeof(struct addrinfo));
    // if(-1 == sock_status)
    // {
    //     perror("socket bind failure: ");
    //     closelog();
    //     freeaddrinfo(servinfo);
    //     return -1;
    // }

    // // free the servinfo
    // freeaddrinfo(servinfo);
    // servinfo = NULL;

    // sock_status = listen(sfd, BACKLOG);
    // if(-1 == sock_status)
    // {
    //     closelog();
    //     perror("socket listen failure: ");
    //     return -1;
    // }

    int sfd = setup_socket();  //Creating a socket
    if (sfd == -1) {
        exit_func();
        return -1;
    }

    /* Open a file */
    int fd = open(SOCKET_ADDR, (O_CREAT | O_TRUNC | O_RDWR), (S_IRWXU | S_IRWXG | S_IROTH));
    if(fd == -1)
    {
        closelog();
        perror("Error opening socket file:");
        return -1;
    }

    /* Mutex init */
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

	/*------------------------------------------------------------------------*/
    /* Timer thread create */
    timer_node_t timer_node;
    timer_node.mutex = &mutex;
    timer_node.fd = fd;
    if(0 != pthread_create(&(timer_node.thread_id), NULL, timer_thread, &timer_node))
    {
        /* Still continue if failure is seen */
        perror("pthread create failed: ");
    }
    else
    {
        syslog(LOG_DEBUG, "Thread create successful. thread = %ld", timer_node.thread_id);
    }

    /* Linked List head */
    SLIST_INIT(&head);
    //node_t *head = NULL;

    struct sockaddr_storage test_addr;          // Test addr to populate from accept()
    socklen_t addr_size = sizeof(test_addr);    // Size of test addr

    while(!complete_exec)
    {
        // Connection with the client
        int client_fd = accept(sfd, (struct sockaddr *)&test_addr, &addr_size);
        if((client_fd == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
        {
            continue;                           // Try again
        }
        else if(client_fd == -1)
        {
            perror("accept failure: ");
            break;
        }   

    datap = (slist_data_t *) malloc(sizeof(slist_data_t));
		SLIST_INSERT_HEAD(&head,datap,entries);

		datap->thread_data.client_fd = client_fd;
		datap->thread_data.complete_flag = 0;
		datap->thread_data.mutex = &mutex;
        datap->thread_data.fd = fd;
        datap->thread_data.client_addr = (struct sockaddr_in *)&test_addr;

		int status = pthread_create(&(datap->thread_data.thread_id), 			
							NULL,			
							thread_socket,			
							&datap->thread_data
							);

		if (status != 0)
		{
			syslog(LOG_ERR,"pthread_create() failed\n");
			return -1;
		}

		syslog(LOG_INFO,"Thread created\n");

		SLIST_FOREACH(datap,&head,entries)
		{
			pthread_join(datap->thread_data.thread_id,NULL);
			if(datap->thread_data.complete_flag == 1)
			{
				//pthread_join(datap->thread_params.thread_id,NULL);
				SLIST_REMOVE(&head, datap, slist_data_s, entries);
				free(datap);
                datap = NULL;
				break;
			}
		}

    }

    //cleanup.
    if(-1 == close(sfd))
    {
        perror("close sfd failed: ");
    }

    if (close(fd) == -1)
    {
        perror("close fd failed: ");
    }

    //unlink file
    if (unlink(SOCKET_ADDR) == -1)
    {
        perror("unlink failed: ");
    }

    /* Thread join timer threads */
    pthread_join(timer_node.thread_id, NULL);

    //Destroy pthread
    pthread_mutex_destroy(&mutex);

    //Close log
    closelog();

    return 0;
}


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

    if (listen(sockfd, BACKLOG) == -1)    //setting the socket to the listening state using the listen function
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

