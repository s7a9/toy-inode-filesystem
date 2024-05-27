#include <iostream>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <memory>

#include "network/network.h"
#include "bytepack/bytepack.h"
#include "filesystem.h"

std::unique_ptr<RemoteDisk> disk;
std::unique_ptr<FileSystem> fs;

constexpr int FLUSH_INTERVAL = 16;

int server_fd = -1;
int flush_counter = FLUSH_INTERVAL;

void* handler(void*);
void SIGINThandler(int);

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <DiskServerAddr> <DiskServerPort> <FSPort>\n";
        return EXIT_FAILURE;
    }
    disk = std::make_unique<RemoteDisk>(argv[1], atoi(argv[2]));
    std::string line;
    std::cout << "Would you like to format the disk? (y/n): ";
    std::getline(std::cin, line);
    fs = std::make_unique<FileSystem>(disk.get(), line == "y");
    int port = atoi(argv[3]);
    server_fd = initialize_server_socket(port);
    if (server_fd < 0) {
        std::cerr << "Error: initialize_server_socket failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Server started on port " << port << std::endl;
    signal(SIGINT, SIGINThandler);
    run_server(server_fd, handler);
}

#define PACK_ERR(err) bytepack_pack(&response, "i", err)

void* handler(void* agrs) {
    client_handler_args_t* client_handler_args = (client_handler_args_t*) agrs;
    int client_fd = client_handler_args->client_fd;
    const char* client_ip = client_handler_args->client_ip;
    int ret = 0;
    bool authenticated = false;
    bytepack_t request;
    bytepack_t response;
    bytepack_init(&request, 512);
    bytepack_init(&response, 512);
    // Receive username
    std::cout << client_ip << " asking for username\n";
    WorkingDir* wd = nullptr;
    bytepack_recv(client_fd, &request);
    if (request.size == 0) {
        authenticated = false;
    } else {
        char username[MAX_USERNAME_LEN];
        bytepack_unpack(&request, "s", username);
        wd = fs->open_working_dir(username);
        if (wd == nullptr) {
            bytepack_pack(&response, "i", ERROR_USER_NOT_FOUND);
            authenticated = false;
        } else {
            bytepack_pack(&response, "i", 0);
            authenticated = true;
            std::cout << client_ip << " Authenticated for: " << username << std::endl;
        }
        bytepack_send(client_fd, &response);
    }
    // Handle requests
    while (authenticated) {
        --flush_counter;
        if (flush_counter < 0) {
            std::cout << "Flushing...\n";
            fs->flush();
            flush_counter = FLUSH_INTERVAL;
        }
        char buffer[std::max(MAX_FILENAME_LEN, MAX_USERNAME_LEN)];
        bytepack_reset(&request);
        bytepack_reset(&response);
        bytepack_recv(client_fd, &request);
        if (request.size == 0) break;
        // std::cout << "Request from " << client_ip << std::endl;
        // bytepack_dbg_print(&request);
        Operation op;
        bytepack_unpack(&request, "i", &op);
        if (op == OP_NOPE) continue;
        if (op == OP_EXIT) break;
        switch (op) {
        case OP_FORMAT: {
            fs->close_working_dir(wd);
            ret = fs->format();
            wd = fs->open_working_dir("root");
            PACK_ERR(ret);
            break;
        } case OP_CREATE: {
            bytepack_unpack(&request, "s", buffer);
            ret = wd->create_file(buffer);
            PACK_ERR(ret);
            break;
        } case OP_MKDIR: {
            bytepack_unpack(&request, "s", buffer);
            ret = wd->create_dir(buffer);
            PACK_ERR(ret);
            break;
        } case OP_RMFILE: {
            bytepack_unpack(&request, "s", buffer);
            ret = wd->remove(buffer);
            PACK_ERR(ret);
            break;
        } case OP_RMDIR: {
            bytepack_unpack(&request, "s", buffer);
            ret = wd->remove_dir(buffer);
            PACK_ERR(ret);
            break;
        } case OP_CD: {
            bytepack_unpack(&request, "s", buffer);
            ret = wd->change_dir(buffer);
            PACK_ERR(ret);
            break;
        } case OP_LS: {
            std::vector<std::string> list;
            ret = wd->list_dir(list);
            PACK_ERR(ret);
            if (ret == 0) {
                bytepack_pack(&response, "l", list.size());
                for (const std::string& name : list) {
                    bytepack_pack(&response, "s", name.c_str());
                }
            } 
            break;
        } case OP_CAT: {
            bytepack_unpack(&request, "s", buffer);
            ret = wd->acquire_file(buffer, false);
            if (ret == 0) {
                if (wd->active_file().inode()->type == TYPE_FILE) {
                    PACK_ERR(ret);
                    std::vector<char> data(wd->active_file().size());
                    wd->active_file().readall(data.data());
                    bytepack_pack(&response, "l", data.size());
                    bytepack_pack_bytes(&response, data.data(), data.size());
                } else {
                    PACK_ERR(ERROR_NOT_FILE);
                }
                wd->release_file();
            } else {
                PACK_ERR(ret);
            }
            break;
        } case OP_WRITE: {
            size_t offset, size;
            bytepack_unpack(&request, "sll", buffer, &offset, &size);
            ret = wd->acquire_file(buffer, true);
            if (ret == 0) {
                if (wd->active_file().inode()->type != TYPE_FILE) {
                    ret = ERROR_NOT_FILE;
                } else {
                    char* data = (char*) malloc(size);
                    bytepack_unpack_bytes(&request, data, &size);
                    ret = (wd->active_file().write(data, size, offset) == size) ? 0 : ERROR_INVALID;
                    free(data);
                }
                wd->release_file();
            }
            PACK_ERR(ret);
            break;
        } case OP_INSERT: {
            size_t offset, size;
            bytepack_unpack(&request, "sll", buffer, &offset, &size);
            ret = wd->acquire_file(buffer, true);
            if (ret == 0) {
                if (wd->active_file().inode()->type != TYPE_FILE) {
                    ret = ERROR_NOT_FILE;
                } else {
                    char* data = (char*) malloc(size);
                    bytepack_unpack_bytes(&request, data, &size);
                    ret = (wd->active_file().insert(data, size, offset) == size) ? 0 : ERROR_INVALID;
                    free(data);
                }
                wd->release_file();
            }
            PACK_ERR(ret);
            break;
        } case OP_DELETE: {
            size_t offset, size;
            bytepack_unpack(&request, "sll", buffer, &offset, &size);
            ret = wd->acquire_file(buffer, true);
            if (ret == 0) {
                if (wd->active_file().inode()->type != TYPE_FILE) {
                    ret = ERROR_NOT_FILE;
                } else {
                    ret = (wd->active_file().remove(size, offset) == size) ? 0 : ERROR_INVALID;
                }
                wd->release_file();
            }
            PACK_ERR(ret);
            break;
        } case OP_TRUNCATE: {
            size_t size;
            bytepack_unpack(&request, "sl", buffer, &size);
            ret = wd->acquire_file(buffer, true);
            if (ret == 0) {
                if (wd->active_file().inode()->type != TYPE_FILE) {
                    ret = ERROR_NOT_FILE;
                } else {
                    ret = wd->active_file().truncate(size) ? 0 : ERROR_INVALID;
                }
                wd->release_file();
            }
            PACK_ERR(ret);
            break;
        } case OP_STAT: {
            bytepack_unpack(&request, "s", buffer);
            ret = wd->acquire_file(buffer, false);
            PACK_ERR(ret);
            if (ret == 0) {
                std::string info = wd->active_file().dump();
                bytepack_pack(&response, "l", info.size() + 1);
                bytepack_pack(&response, "s", info.c_str());
                wd->release_file();
            }
            break;
        } case OP_CHMOD: {
            int mode;
            bytepack_unpack(&request, "si", buffer, &mode);
            ret = wd->chmod(buffer, static_cast<uint16_t>(mode));
            PACK_ERR(ret);
            break;
        } case OP_CHOWN: {
            int owner;
            bytepack_unpack(&request, "si", buffer, &owner);
            ret = wd->chown(buffer, static_cast<uint32_t>(owner));
            PACK_ERR(ret);
            break;
        } case OP_ADDUSER: {
            if (wd->user() != 0) {
                PACK_ERR(ERROR_PERMISSION);
                break;
            }
            bytepack_unpack(&request, "s", buffer);
            uint32_t uid;
            ret = fs->add_user(buffer, uid);
            PACK_ERR(ret);
            if (ret == 0) {
                bytepack_pack(&response, "l", uid);
            }
            break;
        } case OP_LSUSER: {
            std::vector<std::string> list;
            ret = fs->list_users(list);
            PACK_ERR(ret);
            if (ret == 0) {
                bytepack_pack(&response, "l", list.size());
                for (const std::string& name : list) {
                    bytepack_pack(&response, "s", name.c_str());
                }
            }
            break;
        } case OP_READ: {
            size_t offset, size;
            bytepack_unpack(&request, "sll", buffer, &offset, &size);
            ret = wd->acquire_file(buffer, false);
            if (ret == 0) {
                if (wd->active_file().inode()->type != TYPE_FILE) {
                    ret = ERROR_NOT_FILE;
                    PACK_ERR(ret);
                } else {
                    PACK_ERR(ret);
                    std::vector<char> data(size);
                    wd->active_file().read(data.data(), size, offset);
                    bytepack_pack(&response, "l", data.size());
                    bytepack_pack_bytes(&response, data.data(), data.size());
                }
                wd->release_file();
            } else {
                PACK_ERR(ret);
            }
            break;
        } case OP_DELALL: {
            bytepack_unpack(&request, "s", buffer);
            ret = wd->acquire_file(buffer, true);
            if (ret == 0) {
                if (wd->active_file().inode()->type != TYPE_FILE) {
                    ret = ERROR_NOT_FILE;
                } else {
                    ret = wd->active_file().removeall() ? 0 : ERROR_INVALID;
                }
                wd->release_file();
            }
            PACK_ERR(ret);
        } case OP_FLUSH: {
            flush_counter = FLUSH_INTERVAL;
            fs->flush();
            continue;
        } case OP_RENAME:{
            char newname[MAX_FILENAME_LEN];
            bytepack_unpack(&request, "ss", buffer, newname);
            ret = wd->rename(buffer, newname);
            PACK_ERR(ret);
            break;
        } default: {
            PACK_ERR(ERROR_INVALID_OP);
            break;
        }
        }
        // std::cout << "Response to " << client_ip << std::endl;
        // bytepack_dbg_print(&response);
        bytepack_send(client_fd, &response);
    }
    std::cout << "Client disconnected: " << client_ip << std::endl;
    fs->close_working_dir(wd);
    free(client_handler_args);
    close(client_fd);
    bytepack_free(&request);
    bytepack_free(&response);
    return NULL;
};

void SIGINThandler(int) {
    if (server_fd >= 0) {
        std::cout << "*** Ctrl-c hit, shutting down server..." << std::endl;
        close(server_fd);
    }
    exit(EXIT_SUCCESS);
}