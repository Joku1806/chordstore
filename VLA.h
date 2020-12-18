#ifndef VLA_H
#define VLA_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "bytebuffer.h"

typedef union {
    uint8_t byte;
} VLA_data;

typedef struct {
    size_t capacity;
    size_t length;
    VLA_data* items;
} VLA;

VLA* VLA_initialize_with_capacity(size_t cap);
void VLA_insert(VLA* v, VLA_data item);
bytebuffer* VLA_into_bytebuffer(VLA* v);
void VLA_cleanup(VLA* v, void (*handler)(VLA_data));

#endif