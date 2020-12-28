#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include "crud_protocol.h"

hash_packet *get_blank_hash_packet() {
    hash_packet *blank = malloc(sizeof(hash_packet));
    if (blank == NULL) {
        fprintf(stderr, "get_blank_hash_packet() -> malloc(): %s.\n", strerror(errno));
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

hash_packet *initialize_hash_packet_with_values(actions a, bytebuffer *key, bytebuffer *value) {
    hash_packet *pkg = calloc(1, sizeof(hash_packet));
    if (pkg == NULL) {
        fprintf(stderr, "get_blank_hash_packet() -> malloc(): %s.\n", strerror(errno));
        return NULL;
    }

    pkg->action = a;
    pkg->key = key;
    pkg->value = value;

    return pkg;
}

void free_hash_packet(hash_packet *pkg) {
    free_bytebuffer(pkg->key);
    free_bytebuffer(pkg->value);
    free(pkg);
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
        fprintf(stderr, "read_n_bytes_from_file() -> read(): %s\n", strerror(errno));
        free(bytes);
        return NULL;
    }

    return bytes;
}

hash_packet *receive_hash_packet(int socket_fd) {
    hash_packet *pkg = get_blank_hash_packet();

    uint8_t *header = read_n_bytes_from_file(socket_fd, HEADER_SIZE);
    if (header == NULL) {
        fprintf(stderr, "receive_hash_packet(): Couldn't get packet header.\n");
        free_hash_packet(pkg);
        return NULL;
    }

    pkg->reserved = header[0] >> 4;
    pkg->action = header[0] & 0x0f;

    actions ack_masked = pkg->action & 0x07;
    if (ack_masked != GET && ack_masked != SET && ack_masked != DEL) {
        fprintf(stderr, "receive_hash_packet(): Illegal request parameter %#x.\n", pkg->action);
        free_hash_packet(pkg);
        return NULL;
    }

    uint16_t nw_key_length = 0;
    memcpy(&nw_key_length, header + 1, sizeof(uint16_t));
    pkg->key->length = ntohs(nw_key_length);

    uint32_t nw_value_length = 0;
    memcpy(&nw_value_length, header + 3, sizeof(uint32_t));
    pkg->value->length = ntohl(nw_value_length);

    free(header);

    uint8_t *key = read_n_bytes_from_file(socket_fd, pkg->key->length);
    uint8_t *value = read_n_bytes_from_file(socket_fd, pkg->value->length);

    pkg->key->contents = key;
    pkg->key->contents_are_freeable = value == NULL ? 0 : 1;
    pkg->value->contents = value;
    pkg->value->contents_are_freeable = value == NULL ? 0 : 1;

    return pkg;
}

int write_n_bytes_to_file(int socket_fd, uint8_t *bytes, uint32_t amount) {
    uint32_t total_bytes_sent = 0;
    while (amount - total_bytes_sent > 0) {
        ssize_t bytes_sent = send(socket_fd, bytes + total_bytes_sent, amount - total_bytes_sent, 0);
        if (bytes_sent < 0) {
            fprintf(stderr, "write_n_bytes_to_file() -> send(): %s.\n", strerror(errno));
            return -1;
        }
        total_bytes_sent += bytes_sent;
    }

    return 0;
}

int send_hash_packet(int socket_fd, hash_packet *pkg) {
    uint8_t flags = (pkg->reserved << 4) | pkg->action;
    uint16_t nw_key_length = htons((uint16_t)pkg->key->length);
    uint32_t nw_value_length = htonl(pkg->value->length);

    if (write_n_bytes_to_file(socket_fd, &flags, sizeof(uint8_t)) < 0 ||
        write_n_bytes_to_file(socket_fd, (uint8_t *)&nw_key_length, sizeof(uint16_t)) < 0 ||
        write_n_bytes_to_file(socket_fd, (uint8_t *)&nw_value_length, sizeof(uint32_t)) < 0 ||
        write_n_bytes_to_file(socket_fd, pkg->key->contents, pkg->key->length) < 0 ||
        write_n_bytes_to_file(socket_fd, pkg->value->contents, pkg->value->length) < 0) {
        fprintf(stderr, "send_hash_packet(): Failed to send packet.\n");
        return -1;
    }

    return 0;
}