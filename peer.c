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
#include <poll.h>
#include "datastore.h"
#include "VLA.h"
#include "peer.h"

// wird von uthash gebraucht, um Hash Table zu erstellen
client_info *internal_hash_head = NULL;

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

    VLA *pfds_VLA = VLA_initialize_with_capacity(5);
    // add main socket to pfds_VLA set
    VLA_data new_item = {
        .fd_status.fd = listener_fd,
        .fd_status.events = POLLIN,
    };
    VLA_insert(pfds_VLA, new_item);

    while (is_running) {
        struct sockaddr_storage their_address;
        socklen_t addr_size = sizeof their_address;
        int poll_count = poll((struct pollfd *)pfds_VLA->items, pfds_VLA->length, -1);

        if (poll_count == -1) {
            fprintf(stderr, "poll failed %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < pfds_VLA->length; i++) {
            struct pollfd pfds_item = pfds_VLA->items[i].fd_status;
            if (pfds_item.revents & POLLIN) {
                // data is ready to recv() on this socket
                if (pfds_item.fd == listener_fd) {
                    // socket is main socket
                    int connect_fd = accept(listener_fd, (struct sockaddr *)&their_address, &addr_size);
                    if (connect_fd == -1) {
                        if (errno != EINTR) fprintf(stderr, "accept(): %s\n", strerror(errno));
                        continue;
                    }
                    VLA_data new_connection = {
                        .fd_status.fd = connect_fd,
                        .fd_status.events = POLLIN | POLLOUT,
                    };
                    VLA_insert(pfds_VLA, new_connection);
                } else {
                    // socket is not main socket
                    generic_packet *request = read_unknown_packet(pfds_item.fd);
                    if (request == NULL) {
                        fprintf(stderr, "Error while parsing network request.\n");
                        close(pfds_item.fd);
                        continue;
                    }
                    shutdown(pfds_item.fd, SHUT_RD);

                    if (request->type == PROTO_CRUD) {
                        crud_packet *pkg = (crud_packet *)request->contents;
                        uint16_t hash_value = 0;
                        memcpy(&hash_value, pkg->key->contents, sizeof(uint16_t) > pkg->key->length ? pkg->key->length : sizeof(uint16_t));

                        client_info *new = malloc(sizeof(client_info));
                        new->key = hash_value;
                        new->fd = pfds_item.fd;
                        HASH_ADD_INT(internal_hash_head, key, new);

                        if (hash_value < nodes[0].node_id) {
                            // Führe die Request vom Client aus und sende ihm zurück, ob das auch geklappt hat.
                            crud_packet *response = execute_ds_action(pkg);

                            send_crud_packet(pfds_item.fd, response);
                            shutdown(pfds_item.fd, SHUT_WR);
                            close(pfds_item.fd);

                            free_crud_packet(pkg);
                            free_crud_packet(response);
                        } else if (hash_value < nodes[2].node_id) {  // Nachricht ist für den Bereich zuständig, einfach Request an ihn weiterleiten
                            int peer_fd = establish_tcp_connection_from_ip4(nodes[2].node_ip, nodes[2].node_port);
                            if (peer_fd == -1) {
                                fprintf(stderr, "Konnte keine Socket erstellen.\n");
                                exit(EXIT_FAILURE);
                            }

                            send_crud_packet(peer_fd, pkg);

                            crud_packet *response = get_blank_crud_packet();
                            receive_crud_packet(peer_fd, response, READ_CONTROL);
                            send_crud_packet(pfds_item.fd, response);
                            close(pfds_item.fd);
                            free_crud_packet(pkg);
                            free_crud_packet(response);
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
                            free(pkg);
                        }
                    } else if (request->type == PROTO_CHORD) {
                        chord_packet *pkg = (chord_packet *)request->contents;

                        if (pkg->action == REPLY) {
                            client_info *client = NULL;
                            HASH_FIND_INT(internal_hash_head, &pkg->hash_id, client);
                            if (client == NULL) {
                                fprintf(stderr, "Kein Client hat Key %d angefragt. Irgendetwas ist im Ring oder mit dem jeweiligen Client schiefgelaufen.\n", pkg->hash_id);
                                continue;
                                // TODO: bessere Fehlerbehebung, fds schließen etc
                            }
                            int peer_fd = establish_tcp_connection_from_ip4(pkg->node_ip, pkg->node_port);
                            send_crud_packet(peer_fd, client->request);
                            crud_packet *response = get_blank_crud_packet();
                            receive_crud_packet(peer_fd, response, READ_CONTROL);
                            send_crud_packet(client->fd, response);
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
            }
        }
    }

    close(listener_fd);
    ds_destruct();

    return EXIT_SUCCESS;
}