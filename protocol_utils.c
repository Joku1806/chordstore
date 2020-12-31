#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include "protocol_utils.h"

uint8_t* read_n_bytes_from_file(int fd, uint32_t amount) {
    if (amount == 0) return NULL;

    uint8_t* bytes = calloc(1, amount);
    int received_bytes = 0;
    uint32_t total_bytes = 0;

    while ((received_bytes = read(fd, bytes + total_bytes, amount - total_bytes)) > 0) {
        total_bytes += received_bytes;
    }

    if (received_bytes == -1) {
        fprintf(stderr, "read_n_bytes_from_file() -> read(): %s\n", strerror(errno));
        free(bytes);
        return NULL;
    }

    return bytes;
}

int write_n_bytes_to_file(int fd, uint8_t* bytes, uint32_t amount) {
    uint32_t total_bytes_sent = 0;
    while (amount - total_bytes_sent > 0) {
        ssize_t bytes_sent = send(fd, bytes + total_bytes_sent, amount - total_bytes_sent, 0);
        if (bytes_sent < 0) {
            fprintf(stderr, "write_n_bytes_to_file() -> send(): %s.\n", strerror(errno));
            return -1;
        }
        total_bytes_sent += bytes_sent;
    }

    return 0;
}

int establish_tcp_connection_from_ip4(uint32_t ip4, uint16_t port) {
    int socket_fd = -1;
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr = {
            .s_addr = ip4,
        },
        .sin_port = port,
    };
    memset(&address.sin_zero, 0, sizeof(address.sin_zero));

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, PF_INET)) == -1) {
        fprintf(stderr, "socket(): %s\n", strerror(errno));
        return -1;
    }

    if (connect(socket_fd, &address, sizeof(address)) == -1) {
        close(socket_fd);
        fprintf(stderr, "connect(): %s\n", strerror(errno));
        return -1;
    }

    return socket_fd;
}

// Versucht, Hostname und Port zu einer Liste von IP-Adressen aufzulösen und dann
// damit eine Verbindung aufzubauen.
// Wenn eine Verbindung aufgebaut wurde, wird einfach der zugehörige erstellte Socket File Descriptor zurückgegeben.
// Wenn keine Verbindung aufgebaut werden konnte, wird -1 als Fehlercode zurückgegeben.
int establish_tcp_connection(char* host, char* port) {
    struct addrinfo hints, *address_list;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;        // nur IPv4 zulassen
    hints.ai_socktype = SOCK_STREAM;  // rede über TCP mit Server

    int info_success = getaddrinfo(host, port, &hints, &address_list);
    if (info_success != 0) {
        fprintf(stderr, "getaddrinfo(): %s", gai_strerror(info_success));
        return -1;
    }

    int socket_fd = -1;
    for (struct addrinfo* entry = address_list; entry != NULL; entry = entry->ai_next) {
        if ((socket_fd = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol)) == -1) {
            fprintf(stderr, "socket(): %s\n", strerror(errno));
            continue;
        }

        if (connect(socket_fd, entry->ai_addr, entry->ai_addrlen) == -1) {
            close(socket_fd);
            fprintf(stderr, "connect(): %s\n", strerror(errno));
            continue;
        }

        break;
    }
    freeaddrinfo(address_list);

    if (socket_fd < 0) {
        fprintf(stderr, "establish_tcp_connection() in %s:%d - Couldn't establish connection with %s on port %s.\n", __FILE__, __LINE__, host, port);
    }

    return socket_fd;
}

// erstellt eine Verbindungssocket, mit der neue Verbindungen auf localhost:port angenommen werden
int setup_tcp_listener(char* port) {
    struct addrinfo hints, *address_list;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;        // nur IPv4 zulassen
    hints.ai_socktype = SOCK_STREAM;  // rede über TCP mit Clients/andereren Peers
    hints.ai_flags = AI_PASSIVE;      // fülle automatisch mit lokaler IP aus

    int info_success = getaddrinfo(NULL, port, &hints, &address_list);
    if (info_success != 0) {
        fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(info_success));
        exit(EXIT_FAILURE);
    }

    int socket_fd = -1;

    for (struct addrinfo* entry = address_list; entry != NULL; entry = entry->ai_next) {
        if ((socket_fd = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol)) == -1) {
            fprintf(stderr, "socket(): %s\n", strerror(errno));
            continue;
        }

        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) == -1) {
            close(socket_fd);
            fprintf(stderr, "setsockopt(): %s\n", strerror(errno));
            continue;
        }

        if (bind(socket_fd, entry->ai_addr, entry->ai_addrlen) == -1) {
            close(socket_fd);
            fprintf(stderr, "bind(): %s\n", strerror(errno));
            continue;
        }

        break;
    }

    if (listen(socket_fd, 1) == -1) {
        close(socket_fd);
        fprintf(stderr, "listen(): %s\n", strerror(errno));
        return -1;
    }

    freeaddrinfo(address_list);
    return socket_fd;
}

generic_packet* get_blank_unknown_packet() {
    generic_packet* blank = malloc(sizeof(generic_packet));
    blank->contents = NULL;
    blank->type = PROTO_UNDEF;
    return blank;
}

generic_packet* read_unknown_packet(int fd) {
    generic_packet* wrapper = get_blank_unknown_packet();

    uint8_t* control = read_n_bytes_from_file(fd, 1);
    protocol_t proto_type = control[0] & 0x80;

    switch (proto_type) {
        case PROTO_CRUD:
            hash_packet* pkg = get_blank_hash_packet();
            parse_hash_control(fd, pkg, control);
            receive_hash_packet(fd, pkg, SKIP_CONTROL);
            wrapper->type = PROTO_CRUD;
            wrapper->contents = (void*)pkg;
            break;
        case PROTO_CHORD:
            hash_packet* pkg = get_blank_chord_packet();
            parse_chord_control(fd, pkg, control);
            receive_chord_packet(fd, pkg, SKIP_CONTROL);
            wrapper->type = PROTO_CHORD;
            wrapper->contents = (void*)pkg;
            break;
        case PROTO_UNDEF:
            fprintf(stderr, "read_unknown_packet(): Received undefined packet type. This type isn't supported as of yet, but will probably be in the future!\n");
            free(wrapper);
            free(control);
            return NULL;
        default:
            fprintf(stderr, "read_unknown_packet(): Received unknown packet type. Check for errors in your packet sending code!\n");
            free(wrapper);
            free(control);
            return NULL;
    }

    free(control);
    return wrapper;
}