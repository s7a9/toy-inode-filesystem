#pragma once

// Disk interface
#include <string>
#include <cstdint>

constexpr int SECTION_SIZE = 256;

class RemoteDisk {
public:
    RemoteDisk(const char* host, int port);

    ~RemoteDisk();

    int get_disk_info(int* cylinders, int* sectors);

    int clear_disk_section(int cylinder, int sector);
    int read_disk_section(int cylinder, int sector, char* buffer);
    int write_disk_section(int cylinder, int sector, int data_size, const char* data);

    inline int cylinder_num() const {
        return cylinder_num_;
    }

    inline int section_num() const {
        return section_num_;
    }

    inline bool open() const {
        return sockfd_ >= 0;
    }

private:
    bool check_disk_section(int cylinder, int sector);

    int sockfd_;
    std::string host_;
    int port_;
    char* buffer_;
    int cylinder_num_;
    int section_num_;
};