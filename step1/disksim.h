#ifndef DISKSIM_H
#define DISKSIM_H

#define SECTOR_SIZE 256

#include <stdlib.h>
#include <semaphore.h>

#include "../bytepack/bytepack.h"

/// @brief Disk structure
typedef struct {
    int num_cylinders;      // Number of cylinders
    int num_sectors;        // Number of sectors per cylinder
    int sector_move_time;   // Time to move between adjacent sectors
    char *diskfile;         // Pointer to the disk file buffer
    int current_cylinder;   // Current cylinder
    int current_sector;     // Current sector
    int total_time;         // Total time taken to serve requests
    sem_t mutex;            // Mutex to protect the disk structure
} disk_t;

int disk_init(disk_t *disk, const char* filename, int num_cylinders, int num_sectors, int sector_move_time);

/// @brief Simulate the disk serving a request.
/// @param disk 
/// @param request 
/// @return Response to the request. The buffer is fix-sized.
int disk_serve_request(disk_t *disk, bytepack_t *request, bytepack_t *response);

int disk_free(disk_t *disk);

#endif // !DISKSIM_H