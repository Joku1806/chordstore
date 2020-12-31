#ifndef CHORD_PROTOCOL_H
#define CHORD_PROTOCOL_H

#include <netinet/in.h>

#define PACKET_SIZE 10
#define MAX_DATA_ACCEPT 512

typedef enum {
    LOOKUP = 1,
    REPLY = 2,
} chord_action;

typedef struct {
    unsigned int reserved;
    chord_action action;
    uint16_t hash_id;
    uint16_t node_id;
    uint32_t node_ip;
    uint16_t node_port;
} chord_packet;

typedef struct {
    uint32_t node_ip;
    uint16_t node_port;
    uint16_t node_id;
} peer;

// initialization/cleanup functions
chord_packet* get_blank_chord_packet();

// communication/conversion functions
void parse_chord_control(int socket_fd, chord_packet* pkg, uint8_t* control);
void receive_chord_packet(int socket_fd, chord_packet* pkg, parse_mode m);
int send_chord_packet(int socket_fd, chord_packet* pkg);
peer* setup_ring_neighbours(char* information[]);

#endif