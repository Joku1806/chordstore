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

int main(int argc, char **argv) {
    if (argc != 5) {
        printf("Benutzung: %s <Host> <Port> <Aktion> <Key>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *host = argv[1];
    char *port = argv[2];
    char *action = argv[3];
    char *key = argv[4];

    int connect_fd = establish_tcp_connection(host, port);
    if (connect_fd < 0) {
        exit(EXIT_FAILURE);
    }

    bytebuffer *key_buffer = initialize_bytebuffer_with_values((uint8_t *)key, strlen(key));
    key_buffer->contents_are_freeable = 0;  // key liegt auf dem Stack, free() geht also nicht
    bytebuffer *value_buffer;

    crud_action a = 0;
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
    crud_packet *packet = initialize_crud_packet_with_values(a, key_buffer, value_buffer);
    if (send_crud_packet(connect_fd, packet) < 0) {
        fprintf(stderr, "main(): Failed to send packet to server.\n");
        exit(EXIT_FAILURE);
    }
    free_crud_packet(packet);
    shutdown(connect_fd, SHUT_WR);

    // Erhalte Antwort vom Server und gebe im Fall GET auch das
    // Value aus, wenn es eins gibt.
    crud_packet *response = get_blank_crud_packet();
    receive_crud_packet(connect_fd, response, READ_CONTROL);
    if (!(response->action & ACK)) {
        fprintf(stderr, "main(): Request wasn't acknowledged by server, something went wrong on the server side.\n");
        exit(EXIT_FAILURE);
    }

    // Gebe die Antwort nur aus, wenn GET gesetzt ist.
    if (response->action & GET) {
        fwrite(response->value->contents, response->value->length, 1, stdout);
    }

    free_crud_packet(response);
    close(connect_fd);
    return EXIT_SUCCESS;
}