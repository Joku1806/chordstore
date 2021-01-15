#include <math.h>
#include <string.h>
#include <errno.h>
#include "VLA.h"
#include "debug.h"

// Initialisiert einen VLA mit capacity vielen Bytes reserviert.
VLA* VLA_initialize(size_t capacity, size_t item_size) {
    VLA* v = calloc(1, sizeof(VLA));
    if (v == NULL) {
        panic("%s\n", strerror(errno));
    }

    v->capacity = capacity * item_size;
    v->item_size = item_size;
    v->memory = initialize_bytebuffer_with_capacity(capacity * item_size);

    return v;
}

// Vergrößert die capacity des VLA um factor.
void VLA_expand(VLA* v, double factor) {
    // ceil() wird benutzt, damit man bei 1.x Faktoren über die 1er-capacity hinauskommt
    v->capacity = (size_t)ceil(v->capacity * factor);
    v->memory->contents = realloc(v->memory->contents, v->capacity);
    if (v->memory->contents == NULL) {
        panic("%s\n", strerror(errno));
    }
}

// Fügt ein Item ans Ende des VLA hinzu und vergrößert ihn vorher, wenn nötig.
void VLA_insert(VLA* v, void* address, size_t amount) {
    if (v->memory->length + amount * v->item_size >= v->capacity) {
        VLA_expand(v, (double)(v->memory->length + amount * v->item_size) / (double)v->capacity * 1.5);  // 1.5 statt 2, weil es vor allem für viele Items weniger Memory verbraucht und trotzdem genauso gut funktioniert
    }

    memcpy(v->memory->contents + v->memory->length, address, amount * v->item_size);
    v->memory->length += amount * v->item_size;
}

// Löscht das idx'te Item, indem das letzte Item dorthin kopiert und die Länge um v->item_size verringert wird.
// Diese Methode erhält nicht die Reihenfolge der Items, pass also auf, dass das im aufrufenden Code nicht wichtig ist.
void VLA_delete_by_index(VLA* v, size_t idx) {
    if (idx * v->item_size >= v->memory->length) {
        warn("Index %ld is out of bounds for VLA with length=%ld. Skipping deletion, check your indices.\n", idx, v->memory->length / v->item_size);
        return;
    }

    memcpy(v->memory->contents + idx * v->item_size, v->memory->contents + v->memory->length - v->item_size, v->item_size);
    v->memory->length -= v->item_size;
}

// Gibt einen Pointer zu dem idx'ten pollfd struct im VLA zurück
struct pollfd* VLA_get_pollfd(VLA* v, size_t idx) {
    if (idx * v->item_size >= v->memory->length) {
        warn("Index %ld is out of bounds for VLA with length=%ld. Can't get item, check your indices.\n", idx, v->memory->length / v->item_size);
        return NULL;
    }

    return (struct pollfd*)(v->memory->contents + idx * v->item_size);
}

// Erstellt eine Pointer-Kopie des Bytebuffers und vertauscht die freeable-bits,
// sodass der VLA gelöscht werden kann, ohne dass man danach mit dem erstellten Bytebuffer einen use-after-free kriegt
bytebuffer* VLA_into_bytebuffer(VLA* v) {
    bytebuffer* buffer = initialize_bytebuffer_with_values(NULL, 0);
    bytebuffer_shallow_copy(buffer, v->memory);
    bytebuffer_transfer_ownership(buffer, v->memory);
    return buffer;
}

// Führt eine selbst definierte Funktion handler() auf allen Items aus,
// solange handler() != NULL ist und löscht danach den VLA selbst.
void VLA_cleanup(VLA* v, void (*handler)(void*)) {
    if (handler != NULL) {
        for (size_t i = 0; i < v->memory->length; i += v->item_size) {
            handler((uint8_t*)v->memory->contents + i);
        }
    }

    free_bytebuffer(v->memory);
    free(v);
}