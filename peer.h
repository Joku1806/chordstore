#ifndef PEER_H
#define PEER_H

#include <netinet/in.h>
#include "uthash.h"

typedef struct {
    UT_hash_handle hh;
    uint16_t key;
    int fd;
    hash_packet* request;
} client_info;

#endif