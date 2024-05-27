#ifndef NETWORK_H
#define NETWORK_H

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

// ------------ SERVER -------------- //

typedef struct client_handler_args_t {
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];
} client_handler_args_t;

/// @brief The function to create, bind and listen to a server socket.
/// @param port The port number.
/// @return The file descriptor of the server socket.
int initialize_server_socket(int port);

/// @brief Accept client connections and handle them.
/// @param sock_fd 
/// @param handler 
/// @return Error code.
int run_server(int sock_fd, void* (*handler)(void*));

// ------------ CLIENT -------------- //

/// @brief Connect to a server.
/// @param ip The IP address of the server.
/// @param port The port number.
/// @return The file descriptor of the client socket.
int connect_to_server(const char *ip, int port);

#ifdef __cplusplus
}
#endif

#endif //!NETWORK_H
