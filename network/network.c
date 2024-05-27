#include "network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pthread.h>


int initialize_server_socket(int port) {
    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Error: Could not create socket\n");
        exit(EXIT_FAILURE);
    }
    // Set the server address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    // Bind the socket to the server address
    if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error: Could not bind socket\n");
        perror("bind");
        exit(EXIT_FAILURE);
    }
    // Listen for incoming connections
    if (listen(sockfd, 5) < 0) {
        fprintf(stderr, "Error: Could not listen on socket\n");
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

int run_server(int sock_fd, void* (*handler)(void*)) {
    while (1) {
        // Accept a connection
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_sock_fd = accept(sock_fd, (struct sockaddr *) &client_addr, &client_addr_len);
        if (client_sock_fd < 0) {
            fprintf(stderr, "Error: Could not accept connection\n");
            return -1;
        }
        client_handler_args_t *args = (client_handler_args_t *) malloc(sizeof(client_handler_args_t));
        inet_ntop(AF_INET, &client_addr.sin_addr, args->client_ip, INET_ADDRSTRLEN);
        printf("Receiving connection from %s\n", args->client_ip);
        // Handle the connection in a thread
        pthread_t thread;
        args->client_fd = client_sock_fd;
        pthread_create(&thread, NULL, handler, args);
    }
    return 0;
}

int connect_to_server(const char *ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}
