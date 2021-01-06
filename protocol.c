#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include "protocol.h"
#include "debug.h"

chord_packet *get_blank_chord_packet() {
    chord_packet *blank = malloc(sizeof(chord_packet));
    if (blank == NULL) {
        warn("%s\n", strerror(errno));
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

    uint8_t *contents = read_n_bytes_from_file(socket_fd, CHORD_PACKET_SIZE);

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

    struct in_addr ip_wrapper = {
        .s_addr = pkg->node_ip,
    };
    char *ip4_repr = ip4_to_string(&ip_wrapper);
    debug("Got chord packet with action = %#x, Hash ID = %#x, Node IP = %s and Node Port = %d from socket %d.\n", pkg->action, pkg->hash_id, ip4_repr, pkg->node_port, socket_fd);
    free(ip4_repr);
    free(contents);
}

int send_chord_packet(int socket_fd, chord_packet *pkg) {
    uint8_t header = 0x80 | (pkg->reserved << 2) | pkg->action;
    uint16_t nw_hash_id = htons(pkg->hash_id);
    uint16_t nw_node_id = htons(pkg->node_id);
    uint32_t nw_node_ip = htonl(pkg->node_ip);
    uint16_t nw_node_port = htons(pkg->node_port);

    struct in_addr ip_wrapper = {
        .s_addr = pkg->node_ip,
    };
    char *ip4_repr = ip4_to_string(&ip_wrapper);
    debug("Sending chord packet with action = %#x, Hash ID = %#x, Node IP = %s and Node Port = %d over socket %d.\n", pkg->action, pkg->hash_id, ip4_repr, pkg->node_port, socket_fd);
    free(ip4_repr);

    if (write_n_bytes_to_file(socket_fd, &header, sizeof(uint8_t)) < 0 ||
        write_n_bytes_to_file(socket_fd, (uint8_t *)&nw_hash_id, sizeof(uint16_t)) < 0 ||
        write_n_bytes_to_file(socket_fd, (uint8_t *)&nw_node_id, sizeof(uint16_t)) < 0 ||
        write_n_bytes_to_file(socket_fd, (uint8_t *)&nw_node_ip, sizeof(uint32_t)) < 0 ||
        write_n_bytes_to_file(socket_fd, (uint8_t *)&nw_node_port, sizeof(uint16_t)) < 0) {
        warn("Failed to send packet.\n");
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

int peer_stores_hashvalue(peer *peer, uint16_t hash_value) {
    if (peer->area_start > peer->area_stop) return hash_value >= peer->area_start || hash_value <= peer->area_stop;
    return hash_value >= peer->area_start && hash_value <= peer->area_stop;
}

peer *setup_ring_neighbours(char *information[]) {
    // nodes[0]: Eigene Node
    // nodes[1]: Vorgängernode
    // nodes[2]: Nachfolgernode
    peer *nodes = calloc(3, sizeof(peer));

    for (int i = 1; i < 10; i += 3) {
        if (!string_to_uint16(information[i], &nodes[i / 3].node_id)) {
            panic("Error converting node ID.\n");
        }
        if (!string_to_uint16(information[i + 2], &nodes[i / 3].node_port)) {
            panic("Error converting node port.\n");
        }

        struct addrinfo peer_hints, *peer_address_list;
        memset(&peer_hints, 0, sizeof peer_hints);
        peer_hints.ai_family = AF_INET;        // nur IPv4 zulassen
        peer_hints.ai_socktype = SOCK_STREAM;  // rede über TCP mit Server

        int info_success = getaddrinfo(information[i + 1], information[i + 2], &peer_hints, &peer_address_list);
        if (info_success != 0) {
            warn("%s\n", gai_strerror(info_success));
            return NULL;
        }

        nodes[i / 3].node_ip = ((struct sockaddr_in *)peer_address_list->ai_addr)->sin_addr.s_addr;
        debug("initialized nodes[%d] with ID %s and address %s:%s.\n", i / 3, information[i], information[i + 1], information[i + 2]);
    }

    nodes[0].area_start = nodes[1].node_id + 1;
    nodes[0].area_stop = nodes[0].node_id;

    // Es wird nirgendwo der Bereich vom Vorgänger geprüft, einfach auf 0 setzen
    nodes[1].area_start = 0;
    nodes[1].area_stop = nodes[1].node_id;

    nodes[2].area_start = nodes[0].node_id + 1;
    nodes[2].area_stop = nodes[2].node_id;

    return nodes;
}

crud_packet *get_blank_crud_packet() {
    crud_packet *blank = malloc(sizeof(crud_packet));
    if (blank == NULL) {
        warn("%s\n", strerror(errno));
        return NULL;
    }

    blank->reserved = 0;
    blank->action = 0;
    blank->key = initialize_bytebuffer_with_values(NULL, 0);
    blank->key->contents_are_freeable = 0;
    blank->value = initialize_bytebuffer_with_values(NULL, 0);
    blank->value->contents_are_freeable = 0;

    return blank;
}

crud_packet *initialize_crud_packet_with_values(crud_action a, bytebuffer *key, bytebuffer *value) {
    crud_packet *pkg = calloc(1, sizeof(crud_packet));
    if (pkg == NULL) {
        warn("%s\n", strerror(errno));
        return NULL;
    }

    pkg->action = a;
    pkg->key = key;
    pkg->value = value;

    return pkg;
}

void free_crud_packet(crud_packet *pkg) {
    free_bytebuffer(pkg->key);
    free_bytebuffer(pkg->value);
    free(pkg);
}

void parse_crud_control(int socket_fd, crud_packet *pkg, uint8_t *control) {
    pkg->reserved = control[0] >> 4;
    pkg->action = control[0] & 0x0f;
}

void receive_crud_packet(int socket_fd, crud_packet *pkg, parse_mode m) {
    if (m == READ_CONTROL) {
        uint8_t *control = read_n_bytes_from_file(socket_fd, 1);
        parse_crud_control(socket_fd, pkg, control);
        free(control);
    }

    crud_action ack_masked = pkg->action & 0x07;
    if (ack_masked != GET && ack_masked != SET && ack_masked != DEL) {
        free_crud_packet(pkg);
        panic("Illegal request parameter %#x.\n", pkg->action);
    }

    uint8_t *header = read_n_bytes_from_file(socket_fd, CRUD_HEADER_SIZE);
    if (header == NULL) {
        free_crud_packet(pkg);
        panic("Couldn't get packet header.\n");
    }

    uint16_t nw_key_length = 0;
    memcpy(&nw_key_length, header, sizeof(uint16_t));
    pkg->key->length = ntohs(nw_key_length);

    uint32_t nw_value_length = 0;
    memcpy(&nw_value_length, header + 2, sizeof(uint32_t));
    pkg->value->length = ntohl(nw_value_length);

    free(header);

    uint8_t *key = read_n_bytes_from_file(socket_fd, pkg->key->length);
    uint8_t *value = read_n_bytes_from_file(socket_fd, pkg->value->length);

    pkg->key->contents = key;
    pkg->key->contents_are_freeable = value == NULL ? 0 : 1;
    pkg->value->contents = value;
    pkg->value->contents_are_freeable = value == NULL ? 0 : 1;

    debug("Got CRUD packet with action %#x\nKey: %.*s\nValue: %.*s\n", pkg->action, pkg->key->length, (char *)pkg->key->contents, pkg->value->length, (char *)pkg->value->contents);
}

int send_crud_packet(int socket_fd, crud_packet *pkg) {
    uint8_t flags = (pkg->reserved << 4) | pkg->action;
    uint16_t nw_key_length = htons((uint16_t)pkg->key->length);
    uint32_t nw_value_length = htonl(pkg->value->length);

    if (write_n_bytes_to_file(socket_fd, &flags, sizeof(uint8_t)) < 0 ||
        write_n_bytes_to_file(socket_fd, (uint8_t *)&nw_key_length, sizeof(uint16_t)) < 0 ||
        write_n_bytes_to_file(socket_fd, (uint8_t *)&nw_value_length, sizeof(uint32_t)) < 0 ||
        write_n_bytes_to_file(socket_fd, pkg->key->contents, pkg->key->length) < 0 ||
        write_n_bytes_to_file(socket_fd, pkg->value->contents, pkg->value->length) < 0) {
        warn("Failed to send packet.\n");
        return -1;
    }

    return 0;
}

uint8_t *read_n_bytes_from_file(int fd, uint32_t amount) {
    if (amount == 0) return NULL;

    uint8_t *bytes = calloc(1, amount);
    int received_bytes = 0;
    uint32_t total_bytes = 0;

    while ((received_bytes = read(fd, bytes + total_bytes, amount - total_bytes)) > 0) {
        total_bytes += received_bytes;
    }

    if (received_bytes == -1) {
        warn("%s\n", strerror(errno));
        free(bytes);
        return NULL;
    }

    return bytes;
}

int write_n_bytes_to_file(int fd, uint8_t *bytes, uint32_t amount) {
    uint32_t total_bytes_sent = 0;
    while (amount - total_bytes_sent > 0) {
        ssize_t bytes_sent = send(fd, bytes + total_bytes_sent, amount - total_bytes_sent, 0);
        if (bytes_sent < 0) {
            warn("%s\n", strerror(errno));
            return -1;
        }
        total_bytes_sent += bytes_sent;
    }

    return 0;
}

char *ip4_to_string(struct in_addr *ip4) {
    char *ip4_repr = malloc(INET_ADDRSTRLEN);
    inet_ntop(AF_INET, ip4, ip4_repr, INET_ADDRSTRLEN);
    return ip4_repr;
}

int establish_tcp_connection_from_ip4(uint32_t ip4, uint16_t port) {
    int socket_fd = -1;
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr = {
            .s_addr = ip4,
        },
        .sin_port = htons(port),
    };
    memset(&address.sin_zero, 0, sizeof(address.sin_zero));

    char *ip4_repr = ip4_to_string(&address.sin_addr);
    if ((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        warn("%s\n", strerror(errno));
        free(ip4_repr);
        return -1;
    }

    for (size_t i = 0; i < CONNECTION_RETRIES; i++) {
        if (connect(socket_fd, (struct sockaddr *)&address, sizeof(address)) == 0) break;
        if (i == CONNECTION_RETRIES - 1) {
            close(socket_fd);
            panic("Couldn't establish a connection with %s:%d.\n", ip4_repr, port);
        }

        warn("%s. Couldn't establish a connection with %s:%d on try %ld, retrying now.\n", strerror(errno), ip4_repr, port, i + 1);
        struct timespec ts = {
            .tv_sec = CONNECTION_TIMEOUT / 1000,
            .tv_nsec = (CONNECTION_TIMEOUT % 1000) * 1000000,
        };
        nanosleep(&ts, &ts);
    }

    free(ip4_repr);
    return socket_fd;
}

// Versucht, Hostname und Port zu einer Liste von IP-Adressen aufzulösen und dann
// damit eine Verbindung aufzubauen.
// Wenn eine Verbindung aufgebaut wurde, wird einfach der zugehörige erstellte Socket File Descriptor zurückgegeben.
// Wenn keine Verbindung aufgebaut werden konnte, wird -1 als Fehlercode zurückgegeben.
int establish_tcp_connection(char *host, char *port) {
    struct addrinfo hints, *address_list;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;        // nur IPv4 zulassen
    hints.ai_socktype = SOCK_STREAM;  // rede über TCP mit Server

    int info_success = getaddrinfo(host, port, &hints, &address_list);
    if (info_success != 0) {
        warn("%s\n", gai_strerror(info_success));
        return -1;
    }

    int socket_fd = -1;
    for (struct addrinfo *entry = address_list; entry != NULL; entry = entry->ai_next) {
        if ((socket_fd = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol)) == -1) {
            warn("%s\n", strerror(errno));
            continue;
        }

        if (connect(socket_fd, entry->ai_addr, entry->ai_addrlen) == -1) {
            close(socket_fd);
            warn("%s\n", strerror(errno));
            continue;
        }

        break;
    }
    freeaddrinfo(address_list);

    if (socket_fd < 0) {
        warn("Couldn't establish a connection with %s:%s.\n", host, port);
    }

    debug("Established TCP connection with %s:%s on socket %d.\n", host, port, socket_fd);
    return socket_fd;
}

// erstellt eine Verbindungssocket, mit der neue Verbindungen auf localhost:port angenommen werden
int setup_tcp_listener(char *port) {
    struct addrinfo hints, *address_list;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;        // nur IPv4 zulassen
    hints.ai_socktype = SOCK_STREAM;  // rede über TCP mit Clients/andereren Peers
    hints.ai_flags = AI_PASSIVE;      // fülle automatisch mit lokaler IP aus

    int info_success = getaddrinfo(NULL, port, &hints, &address_list);
    if (info_success != 0) {
        panic("%s\n", gai_strerror(info_success));
    }

    int socket_fd = -1;

    for (struct addrinfo *entry = address_list; entry != NULL; entry = entry->ai_next) {
        if ((socket_fd = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol)) == -1) {
            warn("%s\n", strerror(errno));
            continue;
        }

        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) == -1) {
            close(socket_fd);
            warn("%s\n", strerror(errno));
            continue;
        }

        if (bind(socket_fd, entry->ai_addr, entry->ai_addrlen) == -1) {
            close(socket_fd);
            warn("%s\n", strerror(errno));
            continue;
        }

        break;
    }

    if (listen(socket_fd, 10) == -1) {
        close(socket_fd);
        warn("%s\n", strerror(errno));
        return -1;
    }

    debug("Managed to setup TCP listener on port %s!\n", port);
    freeaddrinfo(address_list);
    return socket_fd;
}

generic_packet *get_blank_unknown_packet() {
    generic_packet *blank = malloc(sizeof(generic_packet));
    blank->contents = NULL;
    blank->type = PROTO_UNDEF;
    return blank;
}

generic_packet *read_unknown_packet(int fd) {
    generic_packet *wrapper = get_blank_unknown_packet();

    uint8_t *control = read_n_bytes_from_file(fd, 1);
    protocol_t proto_type = (control[0] & 0x80) >> 7;

    switch (proto_type) {
        case PROTO_CRUD: {
            debug("Identified unknown packet as CRUD packet, now continuing to read.\n");
            crud_packet *pkg = get_blank_crud_packet();
            parse_crud_control(fd, pkg, control);
            receive_crud_packet(fd, pkg, SKIP_CONTROL);
            wrapper->type = PROTO_CRUD;
            wrapper->contents = (void *)pkg;
            break;
        }
        case PROTO_CHORD: {
            debug("Identified unknown packet as CHORD packet, now continuing to read.\n");
            chord_packet *pkg = get_blank_chord_packet();
            parse_chord_control(fd, pkg, control);
            receive_chord_packet(fd, pkg, SKIP_CONTROL);
            wrapper->type = PROTO_CHORD;
            wrapper->contents = (void *)pkg;
            break;
        }
        default:
            free(wrapper);
            free(control);
            panic("Received unknown packet type %#x. Check for errors in your packet sending code!\n", proto_type);
    }

    free(control);
    return wrapper;
}