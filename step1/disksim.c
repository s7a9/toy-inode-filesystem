#include "disksim.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>

#define CHKRET(cond) if ((ret=(cond))<0) { return ret; }

int disk_init(disk_t *disk, const char* filename, int num_cylinders, int num_sectors, int sector_move_time) {
    disk->num_cylinders = num_cylinders;
    disk->num_sectors = num_sectors;
    disk->sector_move_time = sector_move_time;
    disk->current_cylinder = 0;
    disk->current_sector = 0;
    disk->total_time = 0;
    // Open the disk file
    int diskfile_fd = open(filename, O_RDWR | O_CREAT, 0);
    if (diskfile_fd < 0) {
        perror("open");
        return -1;
    }
    // Calculate the size of the disk file
    size_t diskfile_size = num_cylinders * num_sectors * SECTOR_SIZE;
    // Truncate the disk file to the correct size
    if (ftruncate(diskfile_fd, diskfile_size) < 0) {
        perror("ftruncate");
        return -1;
    }
    // Map the disk file to memory
    disk->diskfile = mmap(NULL, diskfile_size, PROT_READ | PROT_WRITE, MAP_SHARED, diskfile_fd, 0);
    if (disk->diskfile == MAP_FAILED) {
        perror("mmap");
        return -1;
    }
    // Initialize the mutex
    if (sem_init(&disk->mutex, 0, 1) < 0) {
        perror("sem_init");
        return -1;
    }
    return 0;
}

void move_head(disk_t *disk, int cylinder) {
    int wait_time = abs(disk->current_cylinder - cylinder) * disk->sector_move_time;
    disk->total_time += wait_time;
    disk->current_cylinder = cylinder;
    usleep(wait_time);
}

#define CHECK_DISK_RANGE \
    if (cylinder < 0 || cylinder >= disk->num_cylinders || sector < 0 || sector >= disk->num_sectors) {\
        bytepack_pack(response, "is", 0, "Error: Cyliner or sector out of range");\
        return -1;\
    }

int disk_read(disk_t *disk, int cylinder, int sector, bytepack_t *response) {
    CHECK_DISK_RANGE;
    move_head(disk, cylinder);
    bytepack_pack(response, "i", SECTOR_SIZE);
    bytepack_pack_bytes(response, disk->diskfile + (cylinder * disk->num_sectors + sector) * SECTOR_SIZE, SECTOR_SIZE);
    return 0;
}

int disk_write(disk_t *disk, int cylinder, int sector, int data_size, const char *data, bytepack_t *response) {
    CHECK_DISK_RANGE;
    if (data_size > SECTOR_SIZE) {
        bytepack_pack(response, "is", 0, "Error: Data size too large");
        return -1;
    }
    move_head(disk, cylinder);
    memcpy(disk->diskfile + (cylinder * disk->num_sectors + sector) * SECTOR_SIZE, data, data_size);
    memset(disk->diskfile + (cylinder * disk->num_sectors + sector) * SECTOR_SIZE + data_size, 0, SECTOR_SIZE - data_size);
    bytepack_pack(response, "i", 1);
    return 0;
}

int disk_clear_section(disk_t *disk, int cylinder, int sector, bytepack_t *response) {
    CHECK_DISK_RANGE;
    move_head(disk, cylinder);
    memset(disk->diskfile + (cylinder * disk->num_sectors + sector) * SECTOR_SIZE, 0, SECTOR_SIZE);
    bytepack_pack(response, "i", 1);
    return 0;
}

int disk_serve_request_(disk_t *disk, bytepack_t *request, bytepack_t *response) {
    int cylinder, sector, data_size, ret;
    // read the request type: I R W
    char request_type;
    CHKRET(bytepack_unpack(request, "c", &request_type));
    if (request_type == 'I') {
        CHKRET(bytepack_pack(response, "ii", disk->num_cylinders, disk->num_sectors));
    } else if (request_type == 'C') {
        CHKRET(bytepack_unpack(request, "ii", &cylinder, &sector));
        CHKRET(disk_clear_section(disk, cylinder, sector, response));
    } else if (request_type == 'R') {
        CHKRET(bytepack_unpack(request, "ii", &cylinder, &sector));
        CHKRET(disk_read(disk, cylinder, sector, response));
    } else if (request_type == 'W') {
        char data[320];
        CHKRET(bytepack_unpack(request, "iii", &cylinder, &sector, &data_size));
        size_t data_real_size;
        // printf("data_size: %d\n", data_size);
        CHKRET(bytepack_unpack_bytes(request, data, &data_real_size));
        // printf("data_real_size: %ld\n", data_real_size);
        // printf("data: %s\n", data);
        if (data_real_size < data_size) {
            CHKRET(bytepack_pack(response, "is", 0, "Error: Data size mismatch"));
        } else {
            CHKRET(disk_write(disk, cylinder, sector, data_size, data, response));
        }
    } else if (request_type == 'E') {
        CHKRET(bytepack_pack(response, "ii", 1, disk->total_time));
        return 'E';
    } else {
        CHKRET(bytepack_pack(response, "is", 0, "Error: Invalid request type"));
    }
    return 0;
}

// Wrapper function to serve request with mutex
int disk_serve_request(disk_t *disk, bytepack_t *request, bytepack_t *response) {
    sem_wait(&disk->mutex);
    int ret = disk_serve_request_(disk, request, response);
    sem_post(&disk->mutex);
    return ret;
}

int disk_free(disk_t *disk) {
    sem_wait(&disk->mutex); // Wait for the disk mutex
    if (munmap(disk->diskfile, disk->num_cylinders * disk->num_sectors * SECTOR_SIZE) < 0) {
        perror("munmap");
        return -1;
    }
    if (sem_destroy(&disk->mutex) < 0) {
        perror("sem_destroy");
        return -1;
    }
    return 0;
}