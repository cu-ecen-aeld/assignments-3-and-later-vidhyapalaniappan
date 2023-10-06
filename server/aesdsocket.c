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

volatile sig_atomic_t exit_flag = 0;

void sig_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_INFO, "plv : Caught signal, exiting");
        exit_flag = 1;
    }
}

int main() {
    // Initialize signal handler
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    // Create and bind the socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt");
        close(sockfd);
        return -1;
    }


    struct sockaddr_in server_addr, client_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 5) == -1) {
        perror("listen");
        close(sockfd);
        return -1;
    }

    syslog(LOG_INFO, "plv : erver is listening on port %d", PORT);

    while (!exit_flag) {
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "plv : Accepted connection from %s", client_ip);

        // Receive data and append to file
        char buffer[1024];
        int data_fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
        ssize_t bytes_received;

        while ((bytes_received = recv(client_fd, buffer, sizeof(buffer), 0)) > 0) {
            write(data_fd, buffer, bytes_received);
            // Check for newline to consider the packet complete
            if (memchr(buffer, '\n', bytes_received) != NULL) {
                break;
            }
        }
	syslog(LOG_INFO, "plv: Received data from %s: %.*s", client_ip, (int)bytes_received, buffer);

        close(data_fd);

        // Read and send data back to the client
        data_fd = open(DATA_FILE, O_RDONLY);
        ssize_t bytes_read;

        while ((bytes_read = read(data_fd, buffer, sizeof(buffer))) > 0) {
            send(client_fd, buffer, bytes_read, 0);
            syslog(LOG_INFO, "plv : Sent data to %s: %.*s", client_ip, (int)bytes_read, buffer);
	}

        close(data_fd);
        close(client_fd);

        syslog(LOG_INFO, "plv : Closed connection from %s", client_ip);
    }

    // Cleanup and exit
    close(sockfd);
    remove(DATA_FILE);
    closelog();
    return 0;
}

