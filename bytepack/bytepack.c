#include "bytepack.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/socket.h>

const char* error_msg = NULL;

int bytepack_init(bytepack_t* bp, size_t bufsize) {
    bp->data = (char*)malloc(bufsize);
    if (bp->data == NULL) {
        error_msg = "Memory allocation failed";
        return -1;
    }
    bp->bufsize = bufsize;
    bp->size = 0;
    bp->offset = 0;
    return 0;
}

int bytepack_attach(bytepack_t* bp, void* data, size_t size) {
    bp->data = data;
    bp->bufsize = size;
    bp->size = size;
    bp->offset = 0;
    return 0;
}

void bytepack_free(bytepack_t* bp) {
    free(bp->data);
    bp->data = NULL;
    bp->bufsize = 0;
    bp->size = 0;
    bp->offset = 0;
}

void bytepack_reset(bytepack_t* bp) {
    bp->size = 0;
    bp->offset = 0;
    memset(bp->data, 0, bp->bufsize);
}

int bytepack_append(bytepack_t* bp, const void* data, size_t size) {
    if (bp->offset + size > bp->bufsize) { // Reallocation
        size_t new_bufsize = bp->bufsize * 2;
        while (bp->offset + size > new_bufsize) {
            new_bufsize *= 2;
        }
        char* new_data = (char*)realloc(bp->data, new_bufsize);
        if (new_data == NULL) {
            error_msg = "Memory allocation failed";
            return -1;
        }
        bp->data = new_data;
        bp->bufsize = new_bufsize;
    }
    memcpy(bp->data + bp->offset, data, size);
    bp->offset += size;
    bp->size = bp->offset;
    return 0;
}

int bytepack_pack(bytepack_t* bp, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = 0;
    for (const char* p = format; *p != '\0' && ret >= 0; p++) {
        if (*p == 'c') {
            char c = (char)va_arg(args, int);
            ret = bytepack_append(bp, &c, sizeof(char));
        } else if (*p == 'i') {
            int i = va_arg(args, int);
            ret = bytepack_append(bp, &i, sizeof(int));
        } else if (*p == 'l') {
            long l = va_arg(args, long);
            ret = bytepack_append(bp, &l, sizeof(long));
        } else if (*p == 's') {
            char* s = va_arg(args, char*);
            size_t len = strlen(s);
            if (ret == 0) {
                ret = bytepack_append(bp, s, len + 1);
            }
        } else {
            error_msg = "Invalid format";
            ret = -1;
        }
    }
    va_end(args);
    if (ret == -1) {
        bytepack_reset(bp);
    }
    return ret;
}

int bytepack_pack_bytes(bytepack_t* bp, const void* data, size_t size) {
    int ret = 0;
    ret = bytepack_append(bp, &size, sizeof(size_t));
    if (ret == 0) {
        ret = bytepack_append(bp, data, size);
    }
    return 0;
}

#define CHECK_UNDERFLOW(TYPE) \
    if (bp->offset + sizeof(TYPE) > bp->size) {\
        error_msg = "Buffer underflow";\
        ret = -1;\
        break;\
    } 

int bytepack_unpack(bytepack_t* bp, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = 0;
    for (const char* p = format; *p != '\0' && ret >= 0; p++) {
        if (*p == 'c') {
            char* c = va_arg(args, char*);
            CHECK_UNDERFLOW(char);
            *c = *(bp->data + bp->offset);
            bp->offset += sizeof(char);
        } else if (*p == 'i') {
            int* i = va_arg(args, int*);
            CHECK_UNDERFLOW(int);
            *i = *(int*)(bp->data + bp->offset);
            bp->offset += sizeof(int);
        } else if (*p == 'l') {
            long* l = va_arg(args, long*);
            CHECK_UNDERFLOW(long);
            *l = *(long*)(bp->data + bp->offset);
            bp->offset += sizeof(long);
        } else if (*p == 's') {
            char* s = va_arg(args, char*);
            size_t len = strlen(bp->data + bp->offset) + 1;
            if (bp->offset + len > bp->size) {
                error_msg = "Buffer underflow";
                ret = -1;
                break;
            }
            memcpy(s, bp->data + bp->offset, len);
            bp->offset += len;
        } else {
            error_msg = "Invalid format";
            ret = -1;
        }
    }
    va_end(args);
    return ret;
}

int bytepack_unpack_bytes(bytepack_t* bp, void* data, size_t* size) {
    if (bp->offset + sizeof(size_t) > bp->size) {
        error_msg = "Buffer underflow";
        return -1;
    }
    *size = *(size_t*)(bp->data + bp->offset);
    bp->offset += sizeof(size_t);
    if (bp->offset + *size > bp->size) {
        error_msg = "Buffer underflow";
        return -1;
    }
    memcpy(data, bp->data + bp->offset, *size);
    bp->offset += *size;
    return 0;
}

int bytepack_send(int sockfd, const bytepack_t* bp) {
    // First send the size, then data
    if (send(sockfd, &bp->size, sizeof(size_t), 0) == -1) {
        error_msg = "Failed to send size";
        return -1;
    }
    if (send(sockfd, bp->data, bp->size, 0) == -1) {
        error_msg = "Failed to send data";
        return -1;
    }
    return 0;
}

int bytepack_recv(int sockfd, bytepack_t* bp) {
    // First receive the size, then data
    if (recv(sockfd, &bp->size, sizeof(size_t), 0) == -1) {
        error_msg = "Failed to receive size";
        return -1;
    }
    if (bp->size > bp->bufsize) { // reallocation
        char* new_data = (char*)realloc(bp->data, bp->size);
        if (new_data == NULL) {
            error_msg = "Memory allocation failed";
            return -1;
        }
        bp->data = new_data;
        bp->bufsize = bp->size;
    }
    // receive data until size is reached
    size_t received = 0;
    while (received < bp->size) {
        ssize_t n = recv(sockfd, bp->data + received, bp->size - received, 0);
        if (n == -1) {
            error_msg = "Failed to receive data";
            return -1;
        }
        received += n;
    }
    bp->offset = 0;
    return 0;
}

const char* bytepack_get_error() {
    return error_msg;
}

void bytepack_dbg_print(const bytepack_t* bp) {
    printf("[size %zu", bp->size);
    for (size_t i = 0; i < bp->size; i++) {
        if (i % 16 == 0) putchar('\n');
        printf("%02x ", (unsigned char)bp->data[i]);
    }
    printf("]\n");
}