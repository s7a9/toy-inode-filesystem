// A basic disk server simulater.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <sys/mman.h>

#include "network/network.h"
#include "disksim.h"

int num_cylinders;      // Number of cylinders
int num_sectors;        // Number of sectors per cylinder
int sector_move_time;   // Time to move between adjacent sectors
int port;               // Port number
int diskfile_fd;        // File descriptor of the disk file
int server_fd;          // File descriptor of the server socket
disk_t disk;            // Disk structure

// The function to handle client requests
void* handler(void*);
// The function to handle SIGINT
void SIGINThandler(int);

int main(int argc, char *argv[]) {
    // Parse the command line arguments
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <diskfilename> <num_cylinders> <num_sectors> <sector_move_time> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    num_cylinders = atoi(argv[2]);
    num_sectors = atoi(argv[3]);
    sector_move_time = atoi(argv[4]);
    port = atoi(argv[5]);
    // check arguments
    if (num_cylinders <= 0 || num_sectors <= 0 || sector_move_time <= 0 || port <= 64 || port >= 65536) {
        fprintf(stderr, "Error: Invalid arguments\n");
        exit(EXIT_FAILURE);
    }
    if (disk_init(&disk, argv[1], num_cylinders, num_sectors, sector_move_time) < 0) {
        fprintf(stderr, "Error: disk_init failed\n");
        exit(EXIT_FAILURE);
    }
    server_fd = initialize_server_socket(port);
    if (server_fd < 0) {
        fprintf(stderr, "Error: initialize_server_socket failed\n");
        exit(EXIT_FAILURE);
    }
    printf("Server started on port %d\n", port);
    signal(SIGINT, SIGINThandler); // Register the signal handler
    run_server(server_fd, handler);
}

void* handler(void* agrs) {
    // Get the client socket file descriptor
    client_handler_args_t* client_handler_args = (client_handler_args_t*) agrs;
    int client_fd = client_handler_args->client_fd;
    const char* client_ip = client_handler_args->client_ip;
    int ret = 0;
    // Serve the request
    bytepack_t request;
    bytepack_t response;
    bytepack_init(&request, 256);
    bytepack_init(&response, 256);
    do {
        // Read the request
        bytepack_reset(&request);
        bytepack_reset(&response);
        // printf("Client %s recv: ", client_ip);
        bytepack_recv(client_fd, &request);
        // bytepack_dbg_print(&request);
        if (request.size == 0) break;
        // Serve the request
        ret = disk_serve_request(&disk, &request, &response);
        if (ret == 'E') break;
        if (ret < 0) {
            const char *error = bytepack_get_error();
            if (error == NULL) error = "Unknown error";
            fprintf(stderr, "%s Error: %s\n", client_ip, error);
        }
        // printf("Client %s send: ", client_ip);
        // bytepack_dbg_print(&response);
        // Send the response
        bytepack_send(client_fd, &response);
    } while (1);
    printf("Client %s disconnected\n", client_ip);
    free(client_handler_args);
    close(client_fd);
    bytepack_free(&request);
    bytepack_free(&response);
    return NULL;
}

void SIGINThandler(int signum) {
    printf("\n*** CTRL-C pressed, doing cleanup...\n");
    // Print statistics
    printf("Total time taken to serve requests: %d\n", disk.total_time);
    disk_free(&disk);
    close(server_fd);
    close(diskfile_fd);
    exit(EXIT_SUCCESS);
}