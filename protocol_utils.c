#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
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