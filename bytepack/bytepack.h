#ifndef BYTEPACK_H
#define BYTEPACK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bytepack_t_ {
    char* data;
    size_t bufsize;
    size_t size;
    size_t offset;
} bytepack_t;

int bytepack_init(bytepack_t* bp, size_t bufsize);

int bytepack_attach(bytepack_t* bp, void* data, size_t size);

void bytepack_free(bytepack_t* bp);

void bytepack_reset(bytepack_t* bp);

int bytepack_pack(bytepack_t* bp, const char* format, ...);

int bytepack_pack_bytes(bytepack_t* bp, const void* data, size_t size);

int bytepack_unpack(bytepack_t* bp, const char* format, ...);

int bytepack_unpack_bytes(bytepack_t* bp, void* data, size_t* size);

int bytepack_send(int sockfd, const bytepack_t* bp);

int bytepack_recv(int sockfd, bytepack_t* bp);

const char* bytepack_get_error();

void bytepack_dbg_print(const bytepack_t* bp);

#ifdef __cplusplus
}
#endif

#endif // !BYTEPACK_H