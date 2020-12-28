#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include "uthash.h"
#include "bytebuffer.h"

#define HEADER_SIZE 7
#define MAX_DATA_ACCEPT 512

typedef enum {
    DEL = 1,
    SET = 2,
    GET = 4,
    ACK = 8,
} actions;

typedef struct {
    unsigned int reserved;
    actions action;
    bytebuffer* key;
    bytebuffer* value;
    UT_hash_handle hh;
} hash_packet;

// initialization/cleanup functions
hash_packet* get_blank_hash_packet();
hash_packet* initialize_hash_packet_with_values(actions a, bytebuffer* key, bytebuffer* value);
void free_hash_packet(hash_packet* pkg);

// communication/conversion functions
uint8_t* read_n_bytes_from_file(int fd, uint32_t amount);
hash_packet* receive_hash_packet(int socket_fd);
int write_n_bytes_to_file(int socket_fd, uint8_t* bytes, uint32_t amount);
int send_hash_packet(int socket_fd, hash_packet* pkg);

#endif