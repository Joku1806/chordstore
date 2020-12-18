#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include "protocol.h"
#include "VLA.h"

// Liest Bytes vom File Descriptor fd, bis die Verbindung beendet wird oder es nichts mehr zu lesen gibt.
bytebuffer *read_from_file(int fd) {
    VLA *stream = VLA_initialize_with_capacity(MAX_DATA_ACCEPT);
    uint8_t *bytes = calloc(1, MAX_DATA_ACCEPT);
    int received_bytes = 0;

    // Es muss hier ein konstant großer Buffer zum initialen Einlesen benutzt werden, weil man vorher nicht wissen kann,
    // ob read() versucht, zu viele Bytes im Buffer abzuspeichern und es somit zu Verlusten kommen kann. Wenn die akzeptierte Anzahl
    // an Bytes aber konstant ist (MAX_DATA_ACCEPT), ist es immer garantiert, dass der Speicherplatz ausreicht.
    // Nach jedem read() müssen also nur noch die gelesen Bytes in einen variablen Buffer kopiert werden. Das ist jetzt möglich,
    // da man durch den Rückgabewert von read() weiß, wie viele Bytes wirklich eingelesen wurden.
    while ((received_bytes = read(fd, bytes, MAX_DATA_ACCEPT)) > 0) {
        // TODO: Finde Weg, wie man den ganzen Block auf einmal einfügen kann. Das wäre in Fällen, in denen eine große
        // Anzahl an Bytes gespeichert werden muss um einiges effizienter, weil im VLA besser vorausgesehen werden kann,
        // wann vergrößert werden muss.
        for (size_t i = 0; i < received_bytes; i++) {
            VLA_data union_pkg = {.byte = bytes[i]};
            VLA_insert(stream, union_pkg);
        }
    }

    if (received_bytes == -1) {
        fprintf(stderr, "read_from_file() -> read(): %s\n", strerror(errno));
        free(bytes);
        VLA_cleanup(stream, NULL);
        return NULL;
    }

    if (stream->length == 0) {
        fprintf(stderr, "read_from_file(): There was nothing to read.\n");
        free(bytes);
        VLA_cleanup(stream, NULL);
        return NULL;
    }

    free(bytes);
    bytebuffer *buffer = VLA_into_bytebuffer(stream);
    VLA_cleanup(stream, NULL);
    return buffer;
}

// Versucht, Hostname und Port zu einer Liste von IP-Adressen aufzulösen und dann
// damit eine Verbindung aufzubauen.
// Wenn eine Verbindung aufgebaut wurde, wird einfach der zugehörige erstellte Socket File Descriptor zurückgegeben.
// Wenn keine Verbindung aufgebaut werden konnte, wird -1 als Fehlercode zurückgegeben.
int establish_socket_connection(char *host, char *port) {
    struct addrinfo hints, *address_list;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;      // egal ob IPv4 oder IPv6
    hints.ai_socktype = SOCK_STREAM;  // rede über TCP mit Server

    int info_success = getaddrinfo(host, port, &hints, &address_list);
    if (info_success != 0) {
        fprintf(stderr, "getaddrinfo(): %s", gai_strerror(info_success));
        return -1;
    }

    int socket_fd = -1;
    for (struct addrinfo *entry = address_list; entry != NULL; entry = entry->ai_next) {
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
        fprintf(stderr, "establish_socket_connection() in %s:%d - Couldn't establish connection with %s on port %s.\n", __FILE__, __LINE__, host, port);
    }

    return socket_fd;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        printf("Benutzung: %s <Host> <Port> <Aktion> <Key>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *host = argv[1];
    char *port = argv[2];
    char *action = argv[3];
    char *key = argv[4];

    int connect_fd = establish_socket_connection(host, port);
    if (connect_fd < 0) {
        exit(EXIT_FAILURE);
    }

    bytebuffer *key_buffer = initialize_bytebuffer_with_values((uint8_t *)key, strlen(key));
    key_buffer->contents_are_freeable = 0;  // key liegt auf dem Stack, free() geht also nicht
    bytebuffer *value_buffer;

    actions a = 0;
    if (strcmp(action, "GET") == 0) {
        a |= GET;
        value_buffer = initialize_bytebuffer_with_values(NULL, 0);
        value_buffer->contents_are_freeable = 0;
    } else if (strcmp(action, "SET") == 0) {
        a |= SET;
        // Nur von stdin lesen, wenn man auch ein value zum Server senden muss.
        // Wenn man das außerhalb von dem if machen würde, würde er bei GET und DELETE in einer Endlosschleife feststecken,
        // weil es halt nichts zu lesen gibt.
        // contents_are_freeable wird hier auf 1 gesetzt, weil in read_from_file() malloc()/realloc() gemacht wird.
        value_buffer = read_from_file(STDIN_FILENO);
        value_buffer->contents_are_freeable = 1;
    } else if (strcmp(action, "DELETE") == 0) {
        a |= DEL;
        value_buffer = initialize_bytebuffer_with_values(NULL, 0);
        value_buffer->contents_are_freeable = 0;
    } else {
        fprintf(stderr, "main(): Illegal action %s.\n", action);
        exit(EXIT_FAILURE);
    }

    // Sende Request zum Server und informiere ihn nach dem Senden mit
    // shutdown(connect_fd, SHUT_WR) darüber, dass man auch fertig geschrieben hat.
    // In manchen Fällen kann es sonst passieren, dass der Server weiter versucht, von der Socket zu lesen,
    // obwohl nichts mehr gesendet wird.
    hash_packet *packet = initialize_hash_packet_with_values(a, key_buffer, value_buffer);
    if (send_hash_packet(connect_fd, packet) < 0) {
        fprintf(stderr, "main(): Failed to send packet to server.\n");
        exit(EXIT_FAILURE);
    }
    free_hash_packet(packet);
    shutdown(connect_fd, SHUT_WR);

    // Erhalte Antwort vom Server und gebe im Fall GET auch das
    // Value aus, wenn es eins gibt.
    hash_packet *response = receive_hash_packet(connect_fd);
    if (!(response->action & ACK)) {
        fprintf(stderr, "main(): Request wasn't acknowledged by server, something went wrong on the server side.\n");
        exit(EXIT_FAILURE);
    }

    // Gebe die Antwort nur aus, wenn GET und ACK beide gesetzt sind.
    if (response->action & GET && response->action & ACK) {
        fwrite(response->value->contents, response->value->length, 1, stdout);
    }

    free_hash_packet(response);
    close(connect_fd);
    return EXIT_SUCCESS;
}