#ifndef PEER_H
#define PEER_H

#include <netinet/in.h>
#include "uthash.h"
#include "protocol.h"

typedef struct {
    UT_hash_handle hh;
    uint16_t key;
    int fd;
    crud_packet* request;
} client_info;

#endif