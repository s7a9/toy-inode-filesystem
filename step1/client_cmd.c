#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../network/network.h"
#include "../bytepack/bytepack.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int server_fd = connect_to_server(argv[1], atoi(argv[2]));
    char data[320];
    bytepack_t request, response;
    bytepack_init(&request, 1024);
    bytepack_init(&response, 1024);
    do {
        printf("Enter a request: ");
        char request_type[5];
        scanf("%s", request_type);
        int result;
        bytepack_reset(&request);
        if (request_type[0] == 'E') {
            bytepack_pack(&request, "c", 'E');
            bytepack_send(server_fd, &request);
            break;
        } else if (request_type[0] == 'R' || request_type[0] == 'C') {
            int cylinder, sector;
            scanf("%d %d", &cylinder, &sector);
            bytepack_pack(&request, "cii", request_type[0], cylinder, sector);
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            bytepack_dbg_print(&response);
            if (result == 0) {
                bytepack_unpack(&response, "s", data);
                printf("No: %s\n", data);
            } else if (request_type[0] == 'R') {
                size_t size;
                bytepack_unpack_bytes(&response, data, &size);
                printf("Yes %s\n", data);
            }
        } else if (request_type[0] == 'W') {
            int cylinder, sector, data_size;
            scanf("%d %d %d ", &cylinder, &sector, &data_size);
            fgets(data, 320, stdin);
            bytepack_pack(&request, "ciii", 'W', cylinder, sector, data_size);
            bytepack_pack_bytes(&request, data, strlen(data) - 1);
            bytepack_dbg_print(&request);
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            if (result == 0) {
                bytepack_unpack(&response, "s", data);
                printf("No: %s\n", data);
            } else {
                printf("Yes\n");
            }
        } else if (request_type[0] == 'I') {
            bytepack_pack(&request, "c", 'I');
            bytepack_send(server_fd, &request);
            bytepack_dbg_print(&request);
            bytepack_recv(server_fd, &response);
            bytepack_dbg_print(&response);
            int r1, r2;
            bytepack_unpack(&response, "ii", &r1, &r2);
            printf("%d %d\n", r1, r2);
        }
    } while (1);
    close(server_fd);
    bytepack_free(&request);
    bytepack_free(&response);
    return 0;
}