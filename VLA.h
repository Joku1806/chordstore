#ifndef VLA_H
#define VLA_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <poll.h>
#include "bytebuffer.h"

typedef enum {
    VLA_UINT8 = sizeof(uint8_t),
    VLA_POLLFD = sizeof(struct pollfd)
} itemsize;

typedef struct {
    size_t capacity;
    itemsize dt_size;
    bytebuffer* memory;
} VLA;

VLA* VLA_initialize(size_t capacity, itemsize dt_size);
void VLA_insert(VLA* v, void* address, size_t amount);
void VLA_delete_by_index(VLA* v, size_t idx);
struct pollfd* VLA_get_pollfd(VLA* v, size_t idx);
bytebuffer* VLA_into_bytebuffer(VLA* v);
void VLA_cleanup(VLA* v, void (*handler)(void*));

#endif