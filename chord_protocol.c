
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
#include <netdb.h>
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

int string_to_uint16(char *src, uint16_t *dest) {
    char *parse_stop;
    errno = 0;
    long converted = strtol(src, &parse_stop, 10);

    // wirft einen Fehler, wenn:
    // 1. in strtol() ein Fehler passiert ist
    // 2. src schon am Anfang keine Zahl ist
    // 3. src nur eine "partielle" Zahl ist (sowas wie 1234asdf)
    // 4. die konvertierte Zahl zu klein/groß für ein uint16_t ist
    if (errno || parse_stop == src || *parse_stop != '\0' || converted < 0 || converted >= 0x10000) return 0;
    *dest = (uint16_t)converted;
    return 1;
}

peer *setup_ring_neighbours(char *information[]) {
    // nodes[0]: Eigene Node
    // nodes[1]: Vorgängernode
    // nodes[2]: Nachfolgernode
    peer nodes[3];

    for (int i = 0; i < 9; i += 3) {
        if (!string_to_uint16(information[i], &nodes[i / 3].node_id)) {
            fprintf(stderr, "Error converting node ID.\n");
            exit(EXIT_FAILURE);
        }
        if (!string_to_uint16(information[i + 2], &nodes[i / 3].node_port)) {
            fprintf(stderr, "Error converting node port.\n");
            exit(EXIT_FAILURE);
        }

        struct addrinfo peer_hints, *peer_address_list;
        memset(&peer_hints, 0, sizeof peer_hints);
        peer_hints.ai_family = AF_INET;        // nur IPv4 zulassen
        peer_hints.ai_socktype = SOCK_STREAM;  // rede über TCP mit Server

        int info_success = getaddrinfo(information[i + 1], information[i + 2], &peer_hints, &peer_address_list);
        if (info_success != 0) {
            fprintf(stderr, "getaddrinfo(): %s", gai_strerror(info_success));
            return -1;
        }

        nodes[i / 3].node_ip = ((struct sockaddr_in *)peer_address_list->ai_addr)->sin_addr.s_addr;
    }

    return nodes;
}