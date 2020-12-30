#ifndef PEER_H
#define PEER_H

#include <netinet/in.h>
#include "uthash.h"

typedef struct {
    uint32_t node_ip;
    uint16_t node_port;
    uint16_t node_id;
} peer;

typedef struct {
    UT_hash_handle hh;
    int key;
    int fd;
    hash_packet* request;
} client_info;

#endif