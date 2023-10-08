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

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024
#define MAX_BACKLOG 5
#define MAX_IP_LEN INET_ADDRSTRLEN

// Error Codes
#define ERROR_SOCKET_CREATE -1
#define ERROR_SOCKET_OPTIONS -2
#define ERROR_SOCKET_BIND   -3
#define ERROR_LISTEN        -4
#define ERROR_FILE_OPEN     -5
#define ERROR_MEMORY_ALLOC  -6

// File Permissions
#define FILE_PERMISSIONS 0644

int sockfd;
int client_fd;

volatile sig_atomic_t exit_flag = 0;

// Function prototypes
static void sig_handler(int signo);
static int daemon_create();
static void cleanup();
static int setup_socket();
static void handle_client_connection(int client_fd);

int main(int argc, char **argv) {
    openlog("aesdsocket", LOG_PID | LOG_NDELAY, LOG_DAEMON);
    syslog(LOG_INFO, "Starting aesdsocket");

    int daemon_fg = 0;

    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_fg = 1;
    } else {
        printf("Invalid argument to process daemon\n");
        syslog(LOG_ERR, "Invalid argument to process daemon");
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    if (daemon_fg) {
        if (daemon_create() == -1) {
            syslog(LOG_ERR, "Error creating Daemon");
        } else {
            syslog(LOG_DEBUG, "Daemon created successfully");
        }
    }

    sockfd = setup_socket();
    if (sockfd == -1) {
        cleanup();
        return ERROR_SOCKET_CREATE;
    }

    syslog(LOG_INFO, "Server is listening on port %d", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            perror("accept");
            syslog(LOG_ERR, "Error accepting client connection: %m");
            continue;
        }

        handle_client_connection(client_fd);
    }

    cleanup();
    return 0;
}

static void sig_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        if (unlink(DATA_FILE) == -1) {
            syslog(LOG_ERR, "Error removing data file: %m");
        }
        close(sockfd);
        close(client_fd);
        exit_flag = 1;
        exit(1);
    }
}

static int daemon_create() {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid != 0) {
        exit(EXIT_SUCCESS);
    }
    
    if (setsid() == -1) {
        perror("setsid");
        return -1;
    }

    if (chdir("/") == -1) {
        perror("chdir");
        return -1;
    }

    int devnull = open("/dev/null", O_RDWR);
    if (devnull == -1) {
        perror("open");
        return -1;
    }
    
    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    
    close(devnull);

    return 0;
}

static void cleanup() {
    close(sockfd);
    if (unlink(DATA_FILE) == -1) {
        syslog(LOG_ERR, "Error removing data file: %m");
    }
    closelog();
}

static int setup_socket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        syslog(LOG_ERR, "Error creating socket: %m");
        return ERROR_SOCKET_CREATE;
    }

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt");
        syslog(LOG_ERR, "Error setting socket options: %m");
        close(sockfd);
        return ERROR_SOCKET_OPTIONS;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        syslog(LOG_ERR, "Error binding socket: %m");
        close(sockfd);
        return ERROR_SOCKET_BIND;
    }

    if (listen(sockfd, MAX_BACKLOG) == -1) {
        perror("listen");
        syslog(LOG_ERR, "Error listening on socket: %m");
        close(sockfd);
        return ERROR_LISTEN;
    }

    return sockfd;
}

static void handle_client_connection(int client_fd) {
    char client_ip[MAX_IP_LEN];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    getpeername(client_fd, (struct sockaddr *)&client_addr, &client_len);
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, MAX_IP_LEN);

    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    char *buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    if (buffer == NULL) {
        printf("Could not allocate memory, exiting ...\n");
        syslog(LOG_ERR, "Could not allocate memory");
        exit(ERROR_MEMORY_ALLOC);
    } else {
        printf("Successfully created buffer\n");
    }

    int data_fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, FILE_PERMISSIONS);
    if (data_fd == -1) {
        perror("open");
        syslog(LOG_ERR, "Error opening data file: %m");
        free(buffer);
        close(client_fd);
        return;
    }

    ssize_t bytes_received;

    while ((bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        write(data_fd, buffer, bytes_received);
        if (memchr(buffer, '\n', bytes_received) != NULL) {
            break;
        }
    }

    syslog(LOG_INFO, "Received data from %s: %s, %d", client_ip, buffer, (int)bytes_received);

    close(data_fd);

    lseek(data_fd, 0, SEEK_SET);

    data_fd = open(DATA_FILE, O_RDONLY);
    if (data_fd == -1) {
        perror("open");
        syslog(LOG_ERR, "Error opening data file for reading: %m");
        free(buffer);
        close(client_fd);
        return;
    }

    ssize_t bytes_read;

    memset(buffer, 0, sizeof(char) * BUFFER_SIZE);
    while ((bytes_read = read(data_fd, buffer, sizeof(char) * BUFFER_SIZE)) > 0) {
        send(client_fd, buffer, bytes_read, 0);
    }

    free(buffer);
    close(data_fd);
    close(client_fd);

    syslog(LOG_INFO, "Closed connection from %s", client_ip);
}

