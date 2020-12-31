#ifndef PROTOCOL_UTILS_H
#define PROTOCOL_UTILS_H

#include <stdint.h>
#include "crud_protocol.h"
#include "chord_protocol.h"

typedef enum {
    PROTO_CRUD = 0,
    PROTO_CHORD = 1,
    PROTO_UNDEF = 2,
} protocol_t;

typedef enum {
    READ_CONTROL = 0,
    SKIP_CONTROL = 1,
} parse_mode;

typedef struct {
    void* contents;
    protocol_t type;
} generic_packet;

uint8_t* read_n_bytes_from_file(int fd, uint32_t amount);
int write_n_bytes_to_file(int fd, uint8_t* bytes, uint32_t amount);
int establish_tcp_connection_from_ip4(uint32_t ip4, uint16_t port);
int establish_tcp_connection(char* host, char* port);
int setup_tcp_listener(char* port);
generic_packet* read_unknown_packet(int fd);

#endif