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
#include <netdb.h>

#define PORT_STR "9000"
#define DATA_FILE "/var/tmp/aesdsocketdata"

int sockfd;
int client_fd;

volatile sig_atomic_t exit_flag = 0;

// Signal handler function
void sig_handler(int signo) {
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

// Function to create a daemon process
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
    
    dup2(devnull, 0);
    dup2(devnull, 1);
    dup2(devnull, 2);
    
    close(devnull);

    return 0;
}

// Cleanup function
void cleanup() {
    close(sockfd);
    if (unlink(DATA_FILE) == -1) {
        syslog(LOG_ERR, "Error removing data file: %m");
    }
    closelog();
}

int main(int argc, char **argv) {
    openlog("aesdsocket", LOG_PID | LOG_NDELAY, LOG_DAEMON);
    syslog(LOG_INFO, "Starting aesdsocket");

    int daemon_fg = 0;
    struct sockaddr_in client_addr;

    if (argc > 1) {
        if (strcmp(argv[1], "-d") == 0) {
            daemon_fg = 1;
        } else {
            printf("Invalid argument to process daemon\n");
            syslog(LOG_ERR, "Invalid argument to process daemon");
        }
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int gai_result = getaddrinfo(NULL, PORT_STR, &hints, &servinfo);
    if (gai_result != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_result));
        syslog(LOG_ERR, "getaddrinfo: %s", gai_strerror(gai_result));
        return -1;
    }

    for (struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("socket");
            syslog(LOG_ERR, "Error creating socket: %m");
            continue;
        }

        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            perror("setsockopt");
            syslog(LOG_ERR, "Error setting socket options: %m");
            close(sockfd);
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("bind");
            syslog(LOG_ERR, "Error binding socket: %m");
            close(sockfd);
            continue;
        }

        break;
    }

    if (servinfo != NULL) {
        freeaddrinfo(servinfo);
    }

    if (sockfd == -1) {
        syslog(LOG_ERR, "Failed to create and bind socket, exiting");
        cleanup();
        return -1;
    }

    if (daemon_fg) {
        if (daemon_create() == -1) {
            syslog(LOG_ERR, "Error creating Daemon");
        } else {
            syslog(LOG_DEBUG, "Daemon created successfully");
        }
    }

    if (listen(sockfd, 5) == -1) {
        perror("listen");
        syslog(LOG_ERR, "Error listening on socket: %m");
        cleanup();
        return -1;
    }

    syslog(LOG_INFO, "Server is listening on port %s", PORT_STR);

    while (1) {
        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            perror("accept");
            syslog(LOG_ERR, "Error accepting client connection: %m");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        char *buffer = (char *)malloc(sizeof(char) * 1024);
        if (buffer == NULL) {
            printf("Could not allocate memory, exiting ...\n");
            syslog(LOG_ERR, "Could not allocate memory");
            exit(1);
        } else {
            printf("Successfully created buffer\n");
        }

        int data_fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (data_fd == -1) {
            perror("open");
            syslog(LOG_ERR, "Error opening data file: %m");
            free(buffer);
            close(client_fd);
            continue;
        }

        ssize_t bytes_received;

        while ((bytes_received = recv(client_fd, buffer, 1024, 0)) > 0) {
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
            continue;
        }

        ssize_t bytes_read;

        memset(buffer, 0, sizeof(char) * 1024);
        while ((bytes_read = read(data_fd, buffer, sizeof(char) * 1024)) > 0) {
            send(client_fd, buffer, bytes_read, 0);
        }

        free(buffer);
        close(data_fd);
        close(client_fd);

        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    cleanup();
    return 0;
}

