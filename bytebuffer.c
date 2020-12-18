#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "bytebuffer.h"

bytebuffer* initialize_bytebuffer_with_values(uint8_t* contents, uint32_t length) {
    bytebuffer* buffer = malloc(sizeof(bytebuffer));
    buffer->contents = contents;
    buffer->contents_are_freeable = 0;  // wird hier nur gesetzt, damit es eine konsistente Vorgehensweise gibt. Für das richtige Setzen ist immer noch der Caller verantwortlich, da C nicht wissen kann, welche Adressen wirklich befreibar sind.
    buffer->length = length;

    return buffer;
}

// Überträgt die Werte von from auf to, ohne die eigentlichen Bytes in bytebuffer->contents zu kopieren.
// Das wird hier so gemacht, weil der Server potentiell viele große Datenmengen (~2^48 Bytes pro Anfrage) verwalten muss.
// Deswegen: weniger Speicher und kopieren -> weniger Auslastung
void bytebuffer_shallow_copy(bytebuffer* to, bytebuffer* from) {
    to->contents = from->contents;
    to->contents_are_freeable = 0;
    to->length = from->length;
}

// Diese Funktion vertauscht die freeable-bits von zwei Bytebuffern, solange diese auf den gleichen Speicher
// zeigen und verschiedene Werte haben. Der Bytebuffer, für den das freeable-Bit auf 1 gesetzt ist, ist verantwortlich dafür,
// dass dieser Bytebuffer am Ende vom Programm freigegeben wird. Wenn eine Menge an Bytebuffern also in contents auf die gleiche Adresse
// zeigt, muss genau einer das freeable-Bit auf 1 gesetzt haben, alles andere ist ein Fehler, weil es sonst
void bytebuffer_transfer_ownership(bytebuffer* buf1, bytebuffer* buf2) {
    if (buf1->contents != buf2->contents) {
        fprintf(stderr, "bytebuffer_transfer_ownership(): Content addresses are not the same.\n");
        exit(EXIT_FAILURE);
    }

    if (buf1->contents_are_freeable == buf2->contents_are_freeable) return;

    int tmp = buf1->contents_are_freeable;
    buf1->contents_are_freeable = buf2->contents_are_freeable;
    buf2->contents_are_freeable = tmp;
}

void print_bytebuffer(bytebuffer* buffer) {
    for (uint32_t i = 0; i < buffer->length; i++) {
        fprintf(stderr, "%c", buffer->contents[i]);
    }
}

void free_bytebuffer(bytebuffer* buffer) {
    if (buffer->contents_are_freeable) free(buffer->contents);
    free(buffer);
}