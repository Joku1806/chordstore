#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "peer.h"
#include "datastore.h"
#include "protocol_utils.h"
#include "crud_protocol.h"
#include "chord_protocol.h"

// wird von uthash gebraucht, um Hash Table zu erstellen
client_info *hash_head = NULL;

int is_running = 1;

// Wird ausgeführt, wenn das Programm ein SIGINT Signal bekommt.
// Diese Funktion setzt is_running auf false, damit nach dem while-loop Handling gemacht werden kann
void close_handler(int num) {
    is_running = 0;
}

int main(int argc, char *argv[]) {
    if (argc != 10) {
        fprintf(stderr, "Benutzung: %s <ID self> <Host self> <Port self>\n\t<ID prev> <Host prev> <Port prev>\n\t<ID next> <Host next> <Port next>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    peer *nodes = setup_ring_neighbours(argv);
    int listener_fd = setup_tcp_listener(argv[3]);
    if (listener_fd == -1) {
        fprintf(stderr, "Konnte keine Verbindungssocket erstellen.\n");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = close_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    while (is_running) {
        struct sockaddr_storage their_address;
        socklen_t addr_size = sizeof their_address;

        int connect_fd = accept(listener_fd, (struct sockaddr *)&their_address, &addr_size);
        if (connect_fd == -1) {
            if (errno != EINTR) fprintf(stderr, "accept(): %s\n", strerror(errno));
            continue;
        }

        generic_packet *request = read_unknown_packet(connect_fd);
        if (request == NULL) {
            fprintf(stderr, "Error while parsing client request.\n");
            close(connect_fd);
            continue;
        }
        shutdown(connect_fd, SHUT_RD);

        if (request->type == PROTO_CRUD) {
            hash_packet *pkg = (hash_packet *)request->contents;
            uint16_t hash_value = 0;
            memcpy(&hash_value, pkg->key->contents, sizeof(uint16_t) > pkg->key->length ? pkg->key->length : sizeof(uint16_t));

            client_info *new = malloc(sizeof(client_info));
            new->key = hash_value;
            new->fd = connect_fd;
            HASH_ADD_INT(hash_head, key, new);

            if (hash_value < nodes[0].node_id) {
                // Führe die Request vom Client aus und sende ihm zurück, ob das auch geklappt hat.
                hash_packet *response = execute_ds_action(request);

                send_hash_packet(connect_fd, response);
                shutdown(connect_fd, SHUT_WR);
                close(connect_fd);

                free_hash_packet(request);
                free_hash_packet(response);
            } else if (hash_value < nodes[2].node_id) {  // Nachricht ist für den Bereich zuständig, einfach Request an ihn weiterleiten
                int peer_fd = establish_tcp_connection_from_ip4(nodes[2].node_ip, nodes[2].node_port);
                if (peer_fd == -1) {
                    fprintf(stderr, "Konnte keine Socket erstellen.\n");
                    exit(EXIT_FAILURE);
                }

                send_hash_packet(peer_fd, request);

                hash_packet *response = get_blank_hash_packet();
                receive_hash_packet(peer_fd, response, READ_CONTROL);
                send_hash_packet(connect_fd, response);
                close(connect_fd);
                free_hash_packet(request);
                free_hash_packet(response);
            } else {  // es ist noch nicht bekannt, wer für den Bereich verantwortlich ist -> lookup machen
                chord_packet *pkg = get_blank_chord_packet();

                pkg->action = LOOKUP;
                pkg->hash_id = hash_value;
                pkg->node_id = nodes[0].node_id;
                pkg->node_ip = nodes[0].node_ip;
                pkg->node_port = nodes[0].node_port;

                int peer_fd = establish_tcp_connection_from_ip4(nodes[2].node_ip, nodes[2].node_port);
                if (peer_fd == -1) {
                    fprintf(stderr, "Konnte keine Socket erstellen.\n");
                    exit(EXIT_FAILURE);
                }

                send_chord_packet(peer_fd, pkg);
                close(peer_fd);
                free_hash_packet(pkg);
            }
        } else if (request->type == PROTO_CHORD) {
            chord_packet *pkg = (chord_packet *)request->contents;

            if (pkg->action == REPLY) {
                client_info *client = NULL;
                HASH_FIND_INT(hash_head, &pkg->hash_id, client);
                if (client == NULL) {
                    fprintf(stderr, "Kein Client hat Key %ld angefragt. Irgendetwas ist im Ring oder mit dem jeweiligen Client schiefgelaufen.\n", pkg->hash_id);
                    continue;
                    // TODO: bessere Fehlerbehebung, fds schließen etc
                }
                int peer_fd = establish_tcp_connection_from_ip4(pkg->node_ip, pkg->node_port);
                send_hash_packet(peer_fd, client->request);
                hash_packet *response = get_blank_hash_packet();
                receive_hash_packet(peer_fd, response, READ_CONTROL);
                send_hash_packet(client->fd, response);
                // TODO: Beenden von mehreren Verbindungen, Löschen von Client/Key mapping, vielleicht eigene Funktion?
            } else if (pkg->action == LOOKUP) {
                if (pkg->hash_id < nodes[2].node_id) {
                    chord_packet *reply = get_blank_chord_packet();

                    reply->action = REPLY;
                    reply->hash_id = pkg->hash_id;
                    reply->node_id = nodes[2].node_id;
                    reply->node_ip = nodes[2].node_ip;
                    reply->node_port = nodes[2].node_port;

                } else {
                    int peer_fd = establish_tcp_connection_from_ip4(nodes[2].node_ip, nodes[2].node_port);
                    if (peer_fd == -1) {
                        fprintf(stderr, "Konnte keine Socket erstellen.\n");
                        exit(EXIT_FAILURE);
                    }

                    send_chord_packet(peer_fd, pkg);
                    close(peer_fd);
                    free(pkg);
                }
            }
        }
    }

    close(listener_fd);
    ds_destruct();

    return EXIT_SUCCESS;
}