#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>

#include "bytepack/bytepack.h"
#include "network/network.h"
#include "errorcode.h"

const char *msg(ecode_t code);
void print_help();

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <ip> <port>" << std::endl;
        return 1;
    }
    int server_fd = connect_to_server(argv[1], atoi(argv[2]));
    std::string line;
    bytepack_t request, response;
    bytepack_init(&request, 1024);
    bytepack_init(&response, 1024);
    // login
    std::cout << "Enter username: ";
    std::getline(std::cin, line);
    bytepack_pack(&request, "s", line.c_str());
    bytepack_send(server_fd, &request);
    bytepack_recv(server_fd, &response);
    ecode_t result;
    bytepack_unpack(&response, "i", &result);
    if (result != 0) {
        std::cerr << msg(result) << std::endl;
        return 1;
    }
    std::cout << "Login successful" << std::endl;
    // main loop
    while (true) {
        std::string cmd;
        std::cout << "\nFS >> ";
        std::cin >> cmd;
        bytepack_reset(&request);
        if (cmd == "format") {
            bytepack_pack(&request, "i", OP_FORMAT);
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "mk") {
            std::string filename;
            std::cin >> filename;
            bytepack_pack(&request, "is", OP_CREATE, filename.c_str());
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "mkdir") {
            std::string dirname;
            std::cin >> dirname;
            bytepack_pack(&request, "is", OP_MKDIR, dirname.c_str());
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        }
        else if (cmd == "rm") {
            std::string filename;
            std::cin >> filename;
            bytepack_pack(&request, "is", OP_RMFILE, filename.c_str());
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "rmdir") {
            std::string dirname;
            std::cin >> dirname;
            bytepack_pack(&request, "is", OP_RMDIR, dirname.c_str());
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "ls") {
            bytepack_pack(&request, "i", OP_LS);
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            if (result != 0) {
                std::cout << msg(result);
            } else {
                size_t count;
                bytepack_unpack(&response, "l", &count);
                std::cout << "total: " << count << std::endl;
                for (size_t i = 0; i < count; ++i) {
                    char name[256];
                    bytepack_unpack(&response, "s", name);
                    std::cout << name << std::endl;
                }
            }
        } else if (cmd == "cd") {
            std::string dirname;
            std::cin >> dirname;
            bytepack_pack(&request, "is", OP_CD, dirname.c_str());
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "chmod") {
            std::string filename;
            int mode;
            std::cin >> filename >> mode;
            bytepack_pack(&request, "isi", OP_CHMOD, filename.c_str(), mode);
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "chown") {
            std::string filename;
            int owner;
            std::cin >> filename >> owner;
            bytepack_pack(&request, "isi", OP_CHOWN, filename.c_str(), owner);
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "rename") {
            std::string oldname, newname;
            std::cin >> oldname >> newname;
            bytepack_pack(&request, "iss", OP_RENAME, oldname.c_str(), newname.c_str());
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "exit" || cmd == "e") {
            bytepack_pack(&request, "i", OP_EXIT);
            bytepack_send(server_fd, &request);
            break;
        } else if (cmd == "cat") {
            std::string filename;
            std::cin >> filename;
            bytepack_pack(&request, "is", OP_CAT, filename.c_str());
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            if (result != 0) {
                std::cout << msg(result);
            } else {
                size_t size;
                bytepack_unpack(&response, "l", &size);
                char *data = new char[size];
                bytepack_unpack_bytes(&response, data, &size);
                std::cout.write(data, size);
                delete[] data;
            }
        } else if (cmd == "w") {
            std::string filename, data;
            size_t offset;
            std::cin >> filename >> offset >> data;
            bytepack_pack(&request, "isll", OP_WRITE, filename.c_str(), offset, data.size());
            bytepack_pack_bytes(&request, data.c_str(), data.size());
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "i") { // insert
            std::string filename, data;
            size_t offset;
            std::cin >> filename >> offset >> data;
            bytepack_pack(&request, "isll", OP_INSERT, filename.c_str(), offset, data.size());
            bytepack_pack_bytes(&request, data.c_str(), data.size());
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "d") { // delete
            std::string filename;
            size_t offset, size;
            std::cin >> filename >> offset >> size;
            bytepack_pack(&request, "isll", OP_DELETE, filename.c_str(), offset, size);
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "stat") {
            std::string filename;
            std::cin >> filename;
            bytepack_pack(&request, "is", OP_STAT, filename.c_str());
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            if (result != 0) {
                std::cout << msg(result);
            } else {
                size_t len;
                bytepack_unpack(&response, "l", &len);
                char *data = new char[len];
                bytepack_unpack(&response, "s", data);
                std::cout << data << std::endl;
                delete[] data;
            }
        } else if (cmd == "trunc") {
            std::string filename;
            size_t size;
            std::cin >> filename >> size;
            bytepack_pack(&request, "isl", OP_TRUNCATE, filename.c_str(), size);
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "del") { // delall
            std::string filename;
            std::cin >> filename;
            bytepack_pack(&request, "is", OP_DELALL, filename.c_str());
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "flush") {
            bytepack_pack(&request, "i", OP_FLUSH);
            bytepack_send(server_fd, &request);
        } else if (cmd == "rn") {
            std::string oldname, newname;
            std::cin >> oldname >> newname;
            bytepack_pack(&request, "iss", OP_RENAME, oldname.c_str(), newname.c_str());
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "adduser") {
            std::string username;
            std::cin >> username;
            bytepack_pack(&request, "is", OP_ADDUSER, username.c_str());
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            std::cout << msg(result);
        } else if (cmd == "r") { // read
            std::string filename;
            size_t offset, size;
            std::cin >> filename >> offset >> size;
            bytepack_pack(&request, "isll", OP_READ, filename.c_str(), offset, size);
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            if (result != 0) {
                std::cout << msg(result);
            } else {
                size_t len;
                bytepack_unpack(&response, "l", &len);
                char *data = new char[len];
                bytepack_unpack_bytes(&response, data, &len);
                std::cout.write(data, len);
                std::cout << std::endl;
                delete[] data;
            }
        } else if (cmd == "lsuser") {
            bytepack_pack(&request, "i", OP_LSUSER);
            bytepack_send(server_fd, &request);
            bytepack_recv(server_fd, &response);
            bytepack_unpack(&response, "i", &result);
            if (result != 0) {
                std::cout << msg(result);
            } else {
                size_t count;
                bytepack_unpack(&response, "l", &count);
                std::cout << "total: " << count;
                for (size_t i = 0; i < count; ++i) {
                    char name[256];
                    bytepack_unpack(&response, "s", name);
                    std::cout << std::endl << name;
                }
            }
        } else if (cmd == "help" || cmd == "h") {
            print_help();
        } else {
            std::cout << "Unknown command: " << cmd << std::endl;
            print_help();
        }
    }
    close(server_fd);
    bytepack_free(&request);
    bytepack_free(&response);
    return 0;
}

const char *msg(ecode_t code) {
    if (code == 0) {
        return "success";
    }
    std::cerr << "Error (" << code << "): ";
    switch (code) {
        case ERROR_PERMISSION: return "Permission denied";
        case ERROR_EXIST: return "File exists";
        case ERROR_NOT_FOUND: return "No such file or directory";
        case ERROR_INVALID_OP: return "Invalid operation";
        case ERROR_NO_SPACE: return "No space left on device";
        case ERROR_NOT_DIR: return "Not a directory";
        case ERROR_NOT_FILE: return "Not a file";
        case ERROR_USER_NOT_FOUND: return "User not found";
        case ERROR_BUSY: return "Device or resource busy";
        default: return "Unknown error";
    }
    return nullptr;
}

void print_help() {
    std::cout << "Commands:\n"
              << "  format\n"
              << "  mk <filename>\n"
              << "  mkdir <dirname>\n"
              << "  rm <filename>\n"
              << "  rmdir <dirname>\n"
              << "  cd <dirname>\n"
              << "  ls\n"
              << "  cat <filename>\n"
              << "  r <filename> <offset> <size>\n"
              << "  w <filename> <offset> <data>: overwrite file\n"
              << "  i <filename> <offset> <data>: insert\n"
              << "  d <filename> <offset> <size>: delete from file\n"
              << "  trunc <filename> <size>: truncate file\n"
              << "  stat <filename>\n"
              << "  chmod <filename> <mode>\n"
              << "  chown <filename> <owner>\n"
              << "  adduser <username>\n"
              << "  lsuser: list all users\n"
              << "  del <filename>: delete all contents in <filename>\n"
              << "  flush: flush cached blocks to disk\n"
              << "  rn <oldname> <newname>\n"
              << "  exit" << std::endl;
}