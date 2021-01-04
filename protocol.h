#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <netinet/in.h>
#include "uthash.h"
#include "bytebuffer.h"

#define CRUD_HEADER_SIZE 6
#define CHORD_PACKET_SIZE 10
#define MAX_DATA_ACCEPT 512
#define CONNECTION_RETRIES 5
#define CONNECTION_TIMEOUT 1000

typedef enum {
    DEL = 1,
    SET = 2,
    GET = 4,
    ACK = 8,
} crud_action;

typedef enum {
    LOOKUP = 1,
    REPLY = 2,
} chord_action;

typedef struct {
    unsigned int reserved;
    crud_action action;
    bytebuffer* key;
    bytebuffer* value;
    UT_hash_handle hh;
} crud_packet;

typedef struct {
    unsigned int reserved;
    chord_action action;
    uint16_t hash_id;
    uint16_t node_id;
    uint32_t node_ip;
    uint16_t node_port;
} chord_packet;

typedef enum {
    READ_CONTROL = 0,
    SKIP_CONTROL = 1,
} parse_mode;

typedef enum {
    PROTO_CRUD = 0,
    PROTO_CHORD = 1,
    PROTO_UNDEF = 2,
} protocol_t;

typedef struct {
    void* contents;
    protocol_t type;
} generic_packet;

typedef struct {
    uint32_t node_ip;
    uint16_t node_port;
    uint16_t node_id;
} peer;

crud_packet* get_blank_crud_packet();
crud_packet* initialize_crud_packet_with_values(crud_action a, bytebuffer* key, bytebuffer* value);
void free_crud_packet(crud_packet* pkg);
void parse_crud_control(int socket_fd, crud_packet* pkg, uint8_t* control);
void receive_crud_packet(int socket_fd, crud_packet* pkg, parse_mode m);
int send_crud_packet(int socket_fd, crud_packet* pkg);

chord_packet* get_blank_chord_packet();
void parse_chord_control(int socket_fd, chord_packet* pkg, uint8_t* control);
void receive_chord_packet(int socket_fd, chord_packet* pkg, parse_mode m);
int send_chord_packet(int socket_fd, chord_packet* pkg);
peer* setup_ring_neighbours(char* information[]);

uint8_t* read_n_bytes_from_file(int fd, uint32_t amount);
int write_n_bytes_to_file(int fd, uint8_t* bytes, uint32_t amount);
char* ip4_to_string(struct in_addr* ip4);
int establish_tcp_connection_from_ip4(uint32_t ip4, uint16_t port);
int establish_tcp_connection(char* host, char* port);
int setup_tcp_listener(char* port);
generic_packet* read_unknown_packet(int fd);

#endif