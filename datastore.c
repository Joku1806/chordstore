#include <stdio.h>
#include "datastore.h"

// wird von uthash gebraucht, um Hash Table zu erstellen
hash_packet *hash_head = NULL;

// Führt die Request vom Client aus und gibt eine Antwort zurück, die dann zum Client zurückgesendet werden kann.
// Die Antwort enthält:
//  - Aktionsbit der Request sowie ACK-Bit, falls Request ausgeführt werden konnte
//  - bei GET: gesetztes Value
//  - bei SET: nichts zusätzliches
//  - bei DEL: nichts zusätzliches
hash_packet *execute_ds_action(hash_packet *pkg) {
    hash_packet *response = get_blank_hash_packet();
    response->action = pkg->action;

    switch (pkg->action) {
        case GET:
            bytebuffer_shallow_copy(response->key, pkg->key);
            hash_packet *entry = ds_query(pkg->key);
            if (entry != NULL) {
                response->action |= ACK;
                bytebuffer_shallow_copy(response->value, entry->value);
            }
            return response;
        case SET:
            ds_set(pkg);
            response->action |= ACK;
            return response;
        case DEL:
            if (ds_delete(pkg->key) >= 0) response->action |= ACK;
            return response;
        default:
            fprintf(stderr, "execute_ds_action(): Illegal request parameter %#x. WARNING: Something is getting through struct un/packing functions!\n", pkg->action);
            return NULL;
    }
}

// überprüft, ob der Key schon im Hash Table existiert und gibt
// das zugehörige struct zurück, falls ja.
// Wenn es das struct nicht gibt, wird NULL zurückgegeben.
hash_packet *ds_query(bytebuffer *key) {
    hash_packet *output = NULL;
    HASH_FIND(hh, hash_head, key->contents, key->length, output);

    fprintf(stderr, "Checking for ");
    print_bytebuffer(key);

    if (output != NULL) {
        fprintf(stderr, ": Found\n");
    } else {
        fprintf(stderr, ": Not found\n");
    }

    return output;
}

// Fügt ein neues struct zum Hash Table hinzu, oder modifiziert den Wert eines structs
// mit dem gleichen Key, falls es so eins gibt.
void ds_set(hash_packet *pkg) {
    hash_packet *entry = ds_query(pkg->key);

    if (!entry) {
        hash_packet *new = get_blank_hash_packet();
        bytebuffer_shallow_copy(new->key, pkg->key);
        bytebuffer_transfer_ownership(new->key, pkg->key);
        bytebuffer_shallow_copy(new->value, pkg->value);
        bytebuffer_transfer_ownership(new->value, pkg->value);
        HASH_ADD_KEYPTR(hh, hash_head, new->key->contents, new->key->length, new);
    } else {
        if (entry->value->contents_are_freeable) free(entry->value->contents);
        bytebuffer_shallow_copy(entry->value, pkg->value);
        bytebuffer_transfer_ownership(entry->value, pkg->value);
    }
}

// Löscht den Pointer zu einem struct aus der Hash Table, wenn eins mit dem gleichen Key gefunden wurde.
// Außerdem wird das struct explizit selbst gelöscht, weil das nicht von uthash übernommen wird.
int ds_delete(bytebuffer *key) {
    hash_packet *entry = ds_query(key);

    if (!entry) {
        return -1;
    } else {
        HASH_DEL(hash_head, entry);
        free_hash_packet(entry);
    }

    return 0;
}

// Löscht alle Pointer zu structs aus der Hash Table, die Hash Table selbst,
// sowie alle structs, die ihm Hash Table gespeichert waren.
void ds_destruct() {
    hash_packet *current, *tmp;

    HASH_ITER(hh, hash_head, current, tmp) {
        HASH_DEL(hash_head, current);
        free_hash_packet(current);
    }
}