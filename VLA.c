#include "VLA.h"

VLA* VLA_initialize_with_capacity(size_t cap) {
    VLA* v = calloc(1, sizeof(VLA));
    v->capacity = cap;
    v->length = 0;
    v->items = calloc(v->capacity, sizeof(VLA_data));

    return v;
}

void VLA_expand(VLA* v, size_t factor) {
    v->capacity *= factor;
    v->items = realloc(v->items, sizeof(VLA_data) * v->capacity);
}

void VLA_insert(VLA* v, VLA_data item) {
    if (v->length == v->capacity) {
        VLA_expand(v, 2);
    }

    v->items[v->length] = item;
    v->length++;
}

bytebuffer* VLA_into_bytebuffer(VLA* v) {
    uint8_t* bytes = calloc(1, v->length);

    for (size_t i = 0; i < v->length; i++) {
        bytes[i] = v->items[i].byte;
    }

    bytebuffer* buffer = initialize_bytebuffer_with_values(bytes, v->length);
    buffer->contents_are_freeable = 1;
    return buffer;
}

void VLA_cleanup(VLA* v, void (*handler)(VLA_data)) {
    if (handler != NULL) {
        for (size_t i = 0; i < v->length; i++) {
            handler(v->items[i]);
        }
    }

    free(v->items);
    free(v);
}