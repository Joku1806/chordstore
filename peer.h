#ifndef PEER_H
#define PEER_H

#include <netinet/in.h>
#include "uthash.h"

typedef struct {
    struct addrinfo* node_address;
    uint16_t node_port;
    uint16_t node_id;
} peer;

typedef struct {
    UT_hash_handle hh;
    uint16_t key;
    int fd;
} keymap;

#endif