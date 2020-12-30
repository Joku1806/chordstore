
/* TODO lookup()

 */
// Joining operations:
/* TODO stabilize()
    returns its predecessor B'=pred(B) to A by sending a notify(B') message
 */
/* TODO notify(B')
    if B' is between A and B, A updates its successor to B'
    A doesnt do anything otherwise
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "protocol_utils.h"
#include "chord_protocol.h"

chord_packet *get_blank_chord_packet() {
    chord_packet *blank = malloc(sizeof(chord_packet));
    if (blank == NULL) {
        fprintf(stderr, "get_blank_chord_packet() -> malloc(): %s.\n", strerror(errno));
        return NULL;
    }

    memset(blank, 0, sizeof(chord_packet));
    return blank;
}

void parse_chord_control(int socket_fd, chord_packet *pkg, uint8_t *control) {
    pkg->reserved = control[0] & 0xfc;
    pkg->action = control[0] & 0x03;
}

void receive_chord_packet(int socket_fd, chord_packet *pkg, parse_mode m) {
    if (m == READ_CONTROL) {
        uint8_t *control = read_n_bytes_from_file(socket_fd, 1);
        parse_chord_control(socket_fd, pkg, control);
        free(control);
    }

    uint8_t *contents = read_n_bytes_from_file(socket_fd, PACKET_SIZE);

    uint16_t nw_hash_id = 0;
    memcpy(&nw_hash_id, contents, sizeof(uint16_t));
    pkg->hash_id = ntohs(nw_hash_id);

    uint16_t nw_node_id = 0;
    memcpy(&nw_node_id, contents + 2, sizeof(uint16_t));
    pkg->node_id = ntohs(nw_node_id);

    uint32_t nw_node_ip = 0;
    memcpy(&nw_node_ip, contents + 4, sizeof(uint32_t));
    pkg->node_ip = (in_addr_t)ntohl(nw_node_ip);

    uint16_t nw_node_port = 0;
    memcpy(&nw_node_port, contents + 8, sizeof(uint16_t));
    pkg->node_port = ntohs(nw_node_port);

    free(contents);
}

int send_chord_packet(int socket_fd, chord_packet *pkg) {
    uint8_t header = 0x80 | (pkg->reserved << 2) | pkg->action;
    uint16_t nw_hash_id = htons(pkg->hash_id);
    uint16_t nw_node_id = htons(pkg->node_id);
    uint32_t nw_node_ip = htonl(pkg->node_ip);
    uint16_t nw_node_port = htons(pkg->node_port);

    if (write_n_bytes_to_file(socket_fd, &header, sizeof(uint8_t)) < 0 ||
        write_n_bytes_to_file(socket_fd, (uint8_t *)&nw_hash_id, sizeof(uint16_t)) < 0 ||
        write_n_bytes_to_file(socket_fd, (uint8_t *)&nw_node_id, sizeof(uint16_t)) < 0 ||
        write_n_bytes_to_file(socket_fd, (uint8_t *)&nw_node_ip, sizeof(uint32_t)) < 0 ||
        write_n_bytes_to_file(socket_fd, (uint8_t *)&nw_node_port, sizeof(uint16_t)) < 0) {
        fprintf(stderr, "send_chord_packet(): Failed to send packet.\n");
        return -1;
    }

    return 0;
}