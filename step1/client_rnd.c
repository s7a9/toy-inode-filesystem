#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int server_fd = connect_to_server(argv[1], atoi(argv[2]));
    char response[MAX_RESPONSE_LEN];
    char *request = NULL;
    do { // Generate random data to test the server
        size_t n = 0, buffer_size;
        n = rand() % 100 + 1;
        request = malloc(n + 1);
        for (size_t i = 0; i < n; i++) {
            request[i] = 'A' + rand() % 26;
        }
        request[n] = '\0';
        n = request_server(server_fd, request, response);
        while (n > 0 && (response[n - 1] == '\n' || response[n - 1] == '\r')) {
            response[--n] = '\0';
        }
        printf("Request [%ld]: %s\n", n, request);
        printf("Response [%ld]: %s\n", n, response);
        free(request);
    } while (request[0] != 'E');
    free(request);
    close(server_fd);
    return 0;
}