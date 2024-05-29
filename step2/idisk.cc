#include "idisk.h"

#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <iostream>

#include "bytepack/bytepack.h"
#include "network/network.h"

constexpr int BUFFER_SIZE = 2 * SECTION_SIZE;
static char error_msg[1024];

RemoteDisk::RemoteDisk(const char* host, int port):
    host_(host), port_(port) {
    sockfd_ = connect_to_server(host, port);
    if (sockfd_ < 0) {
        std::cerr << "Failed to connect to disk on " << host << ":" << port << std::endl;
        return;
    }
    buffer_ = new char[BUFFER_SIZE];
    get_disk_info(nullptr, nullptr);
    std::cout << "Connected to disk on " << host << ":" << port
        << " with " << cylinder_num_ << " cylinders and " << section_num_
        << " sectors" << std::endl;
}

RemoteDisk::~RemoteDisk() {
    bytepack_t bytepack;
    bytepack_attach(&bytepack, buffer_, BUFFER_SIZE);
    bytepack_pack(&bytepack, "c", 'E');
    bytepack_send(sockfd_, &bytepack);
    close(sockfd_);
    delete[] buffer_;
}

int RemoteDisk::get_disk_info(int* cylinders, int* sectors) {
    bytepack_t bytepack;
    bytepack_attach(&bytepack, buffer_, BUFFER_SIZE);
    bytepack_pack(&bytepack, "c", 'I');
    bytepack_send(sockfd_, &bytepack);
    bytepack_reset(&bytepack);
    bytepack_recv(sockfd_, &bytepack);
    bytepack_unpack(&bytepack, "ii", &cylinder_num_, &section_num_);
    if (cylinders)
        *cylinders = cylinder_num_;
    if (sectors)
        *sectors = section_num_;
    return 0;
}

int RemoteDisk::clear_disk_section(int cylinder, int sector) {
    if (!check_disk_section(cylinder, sector)) {
        std::cerr << "Invalid disk section " << cylinder << ":" << sector << std::endl;
        return -1;
    }
    bytepack_t bytepack;
    bytepack_attach(&bytepack, buffer_, BUFFER_SIZE);
    bytepack_pack(&bytepack, "cii", 'C', cylinder, sector);
    bytepack_send(sockfd_, &bytepack);
    bytepack_reset(&bytepack);
    bytepack_recv(sockfd_, &bytepack);
    int ret;
    bytepack_unpack(&bytepack, "i", &ret);
    if (ret == 0) {
        std::cerr << "Failed to clear disk section " << cylinder << ":" << sector << std::endl;
    }
    return ret;
}

int RemoteDisk::read_disk_section(int cylinder, int sector, char* buffer) {
    if (!check_disk_section(cylinder, sector)) {
        std::cerr << "Invalid disk section " << cylinder << ":" << sector << std::endl;
        return -1;
    }
    bytepack_t bytepack;
    bytepack_attach(&bytepack, this->buffer_, BUFFER_SIZE);
    bytepack_pack(&bytepack, "cii", 'R', cylinder, sector);
    bytepack_send(sockfd_, &bytepack);
    bytepack_reset(&bytepack);
    bytepack_recv(sockfd_, &bytepack);
    int sector_size;
    bytepack_unpack(&bytepack, "i", &sector_size);
    if (sector_size == 0) {
        bytepack_unpack(&bytepack, "s", error_msg);
        std::cerr << "Failed to read disk section " << cylinder << ":" << sector <<
            " with error: " << error_msg << std::endl;
        return -1;
    }
    size_t data_size = 0;
    bytepack_unpack_bytes(&bytepack, buffer, &data_size);
    return static_cast<size_t>(sector_size) != data_size ? -1 : 0;
}

int RemoteDisk::write_disk_section(int cylinder, int sector, int data_size, const char* data) {
    if (!check_disk_section(cylinder, sector)) {
        std::cerr << "Invalid disk section " << cylinder << ":" << sector << std::endl;
        return -1;
    }
    bytepack_t bytepack;
    bytepack_attach(&bytepack, buffer_, BUFFER_SIZE);
    // size_t send_size = 277, byte_size = data_size;
    // send(sockfd_, &send_size, sizeof(send_size), 0);
    // send(sockfd_, "W", 1, 0);
    // send(sockfd_, &cylinder, sizeof(cylinder), 0);
    // send(sockfd_, &sector, sizeof(sector), 0);
    // send(sockfd_, &data_size, sizeof(data_size), 0);
    // send(sockfd_, &byte_size, sizeof(byte_size), 0);
    // send(sockfd_, data, data_size, 0);
    bytepack_pack(&bytepack, "ciii", 'W', cylinder, sector, data_size);
    bytepack_pack_bytes(&bytepack, data, data_size);
    bytepack_send(sockfd_, &bytepack);
    bytepack_reset(&bytepack);
    bytepack_recv(sockfd_, &bytepack);
    int ret;
    bytepack_unpack(&bytepack, "i", &ret);
    if (ret == 0) {
        bytepack_unpack(&bytepack, "s", error_msg);
        std::cerr << "Failed to write disk section " << cylinder << ":" << sector <<
            " with error: " << error_msg << std::endl;
    }
    return ret;
}

bool RemoteDisk::check_disk_section(int cylinder, int sector) {
    if (cylinder < 0 || sector < 0) {
        return false;
    }
    return cylinder < cylinder_num_ && sector < section_num_;
}