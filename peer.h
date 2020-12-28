#ifndef PEER_H
#define PEER_H

#include <netinet/in.h>

typedef struct {
    in_addr_t node_ip;
    uint16_t node_port;
    uint16_t node_id;
} peer;

#endif