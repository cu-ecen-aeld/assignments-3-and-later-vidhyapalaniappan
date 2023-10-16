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
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024
#define MAX_BACKLOG 15
#define MAX_IP_LEN INET_ADDRSTRLEN
#define PERIOD 10


/*Defining ERROR codes*/
#define ERROR_SOCKET_CREATE  -1
#define ERROR_SOCKET_OPTIONS -2
#define ERROR_SOCKET_BIND    -3
#define ERROR_LISTEN         -4
#define ERROR_FILE_OPEN      -5
#define ERROR_MEMORY_ALLOC   -6

int execution_flag = 0;

int sock_fd;
int client_fd;
int data_fd;

/* Function prototypes */
static void signal_handler(int signo);
static int daemon_func();
static int setup_socket();
static void exit_func();
void *multi_thread_handler(void *thread_params);
void *timer_thread(void *thread_params);

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

int main(int argc, char **argv)
{
    openlog(NULL, 0, LOG_USER);
    syslog(LOG_INFO, "Starting aesdsocket");
    
    struct sigaction action;

    action.sa_flags = 0;
    action.sa_handler = &signal_handler;

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

    int sock_fd = setup_socket();  //Creating a socket
    if (sock_fd == -1) {
        exit_func();
        return ERROR_SOCKET_CREATE;
    }
    syslog(LOG_INFO, "Server is listening on port %d", PORT);

    /* Open a file */
    data_fd = open(DATA_FILE, (O_CREAT | O_TRUNC | O_RDWR), (S_IRWXU | S_IRWXG | S_IROTH));
    if(data_fd == -1)
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
    timer_node.fd = data_fd;
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
   // socklen_t addr_size = sizeof(test_addr);    // Size of test addr

    while(!execution_flag)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &client_len);  //continuously accepting incoming client connections 
        if (client_fd == -1) {
            perror("accept");
            syslog(LOG_ERR, "Error accepting client connection: %m");
            continue;
        }

        datap = (slist_data_t *) malloc(sizeof(slist_data_t));
		SLIST_INSERT_HEAD(&head,datap,entries);

		datap->thread_data.client_fd = client_fd;
		datap->thread_data.complete_flag = 0;
		datap->thread_data.mutex = &mutex;
        datap->thread_data.fd = data_fd;
        datap->thread_data.client_addr = (struct sockaddr_in *)&test_addr;

		int status = pthread_create(&(datap->thread_data.thread_id), 			
							NULL,			
							multi_thread_handler,			
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

     if(-1 == close(sock_fd))
    {
        perror("close sock_fd failed: ");
    }

    if (close(data_fd) == -1)
    {
        perror("close fd failed: ");
    }

    //unlink file
    if (unlink(DATA_FILE) == -1)
    {
        perror("unlink failed: ");
    }

    /* Thread join timer threads */
    pthread_join(timer_node.thread_id, NULL);

    //Destroy pthread
    pthread_mutex_destroy(&mutex);

    //Close log
    closelog();

    //exit_func();
    return 0;
}

static void signal_handler(int signo)
{
    if((signo == SIGINT) || signo == SIGTERM)
    {
        execution_flag = 1;
    }
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

static void exit_func() 
{
    if(close(sock_fd) == -1)
    {
    	syslog(LOG_ERR, "Error closing data file");
    }
    if (unlink(DATA_FILE) == -1) {
        syslog(LOG_ERR, "Error removing data file: %m");
    }
    // if(close(data_fd) == -1)
    // {
    // 	syslog(LOG_ERR, "Error closing client fd");
    // }
    closelog();
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


void *multi_thread_handler(void *thread_params)
{
    char *recv_buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);   //buffer used to read and write data to and from the client
    if (recv_buffer == NULL) 
    {
        syslog(LOG_ERR, "Could not allocate memory");
        exit(ERROR_MEMORY_ALLOC);
    } else 
    {
        syslog(LOG_ERR, "Successfully created buffer\n");
    }
    memset(recv_buffer, 0, 1024);
    ssize_t bytes_received;


    char *send_buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);   //buffer used to read and write data to and from the client
    if (send_buffer == NULL) 
    {
        syslog(LOG_ERR, "Could not allocate memory");
        exit(ERROR_MEMORY_ALLOC);
    } else 
    {
        syslog(LOG_ERR, "Successfully created buffer\n");
    }
        
    memset(send_buffer, 0, 1024);
    ssize_t bytes_read;

    //int ret;
    // receive bytes
	//ssize_t recv_bytes = 0;
	//char recv_buf[1024];
	
	// send bytes
	//size_t send_bytes = 0;
	//char send_buf[1024];
	//ssize_t bytes_read;



    thread_data_t *thread_data = (thread_data_t*)thread_params;

    char client_ip[MAX_IP_LEN];                   //to store information about the client's IP address. 
    struct sockaddr_in client_addr;               //structure to hold client socket address information
    socklen_t client_len = sizeof(client_addr);

    getpeername(client_fd, (struct sockaddr *)&client_addr, &client_len);  //retrieve the IP address of the connected client
    
    //extracting the client's IP address from the client_addr structure and converting it from binary form to a string using inet_ntop.
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, MAX_IP_LEN);

    syslog(LOG_INFO, "Accepted connection from %s", client_ip);
    
    if (pthread_mutex_lock(thread_data->mutex) == -1)
    {
        syslog(LOG_ERR,"mutex lock failed\n");
        return NULL;
    }

    // receive and write
    while((bytes_received = recv(thread_data->client_fd, recv_buffer, 1024, 0))>0)
    {
        if (write(data_fd, recv_buffer, bytes_received) == -1)
        {
            syslog(LOG_ERR,"File write failed");
            return NULL;
        }
        if (memchr(recv_buffer, '\n', bytes_received) != NULL) //checking if the received data contains a newline character 
        {
            break;
        }
    }

    if (pthread_mutex_unlock(thread_data->mutex) == -1)
    {
        syslog(LOG_ERR,"mutex unlock failed\n");
        return NULL;
    }

    if (pthread_mutex_lock(thread_data->mutex) == -1)
    {
        syslog(LOG_ERR,"mutex lock failed\n");
        return NULL;
    }

    if (lseek(data_fd, 0, SEEK_SET) == -1)
    {
        syslog(LOG_ERR,"lseek failed");
        return NULL;
    }

    // read and send
    while((bytes_read = read(data_fd, send_buffer, 1024) )> 0)
    {
        if (send(thread_data->client_fd, send_buffer, bytes_read, 0) == -1)
        {
            syslog(LOG_ERR,"Send failed");
            return NULL;
        }
    }

    if (pthread_mutex_unlock(thread_data->mutex) == -1)
    {
        syslog(LOG_ERR,"mutex unlock failed\n");
        return NULL;
    }

    close(thread_data->client_fd);
    thread_data->complete_flag = 1;
    return thread_params;
}

void *timer_thread(void *thread_params)
{
    if (thread_params == NULL)
    {
        return NULL;
    }

    timer_node_t *thread_data = (timer_node_t *)thread_params;
    struct timespec ts;
    time_t current_time;
    struct tm *time_info;

    while (!execution_flag)
    {
        char time_buffer[BUFFER_SIZE] = {0};

        if (0 != clock_gettime(CLOCK_MONOTONIC, &ts))
        {
            perror("Clock get time failed: ");
            break;
        }
        ts.tv_sec += PERIOD;

        // Sleep until the next time interval
        if (0 != clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL))
        {
            perror("clock nanosleep failed: ");
            break;
        }

        // Check for termination signal
        if (execution_flag)
        {
            pthread_exit(NULL);
        }

        // Get the current time
        current_time = time(NULL);
        if (current_time == ((time_t)-1))
        {
            perror("time since epoch failed: ");
            break;
        }

        // Format the time
        time_info = localtime(&current_time);
        if (time_info == NULL)
        {
            perror("Local time failed: ");
            break;
        }

        int len = strftime(time_buffer, BUFFER_SIZE, "Custom timestamp: %Y, %b, %d, %H:%M:%S\n", time_info);
        if (len == 0)
        {
            syslog(LOG_ERR, "Failed to get time");
        }

        // Write the formatted time to a file with mutex protection
        if (0 != pthread_mutex_lock(thread_data->mutex))
        {
            perror("Mutex lock failed: ");
            break;
        }

        if (-1 == write(thread_data->fd, time_buffer, len))
        {
            perror("write timestamp failed: ");
        }

        if (pthread_mutex_unlock(thread_data->mutex) != 0)
        {
            perror("Mutex unlock failed: ");
            break;
        }
    }

    return NULL;
}


