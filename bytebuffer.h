#ifndef BYTEBUFFER_H
#define BYTEBUFFER_H

#include <stdint.h>

typedef struct {
    uint8_t* contents;
    uint contents_are_freeable : 1;  // speichert, ob beim Löschen eines erstellten structs die Adresse in contents an free weitergegeben werden muss (zB. nicht bei Stack-Adressen, große reservierte Blöcke über mehrere bytebuffer, etc)
    size_t length;                   // größter Wert, der hier potentiell gespeichert werden muss ist die Länge vom value, deswegen benutze ich hier nur uint32_t, und nicht so etwas wie __uint128_t.
} bytebuffer;

bytebuffer* initialize_bytebuffer_with_capacity(size_t capacity);
bytebuffer* initialize_bytebuffer_with_values(uint8_t* contents, uint32_t length);
void bytebuffer_shallow_copy(bytebuffer* to, bytebuffer* from);
void bytebuffer_transfer_ownership(bytebuffer* buf1, bytebuffer* buf2);
void print_bytebuffer(bytebuffer* buffer);
void free_bytebuffer(bytebuffer* buffer);

#endif