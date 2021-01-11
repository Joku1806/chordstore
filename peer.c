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
#include "debug.h"

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

    int id_length = strlen(argv[1]);
    dbg_identifier = malloc(5 + id_length + 1);
    if (dbg_identifier == NULL) {
        panic("%s\n", strerror(errno));
    }
    strncpy(dbg_identifier, "Peer ", 5);
    strncpy(dbg_identifier + 5, argv[1], id_length);
    dbg_identifier[5 + id_length] = '\0';

    peer *nodes = setup_ring_neighbours(argv);
    int listener_fd = setup_tcp_listener(argv[3]);
    if (listener_fd == -1) {
        panic("Konnte keine Verbindungssocket erstellen.\n");
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
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
            panic("%s\n", strerror(errno));
        }

        for (int i = 0; i < pfds_VLA->length; i++) {
            struct pollfd pfds_item = pfds_VLA->items[i].fd_status;
            if (pfds_item.revents & POLLIN) {
                // data is ready to recv() on this socket
                if (pfds_item.fd == listener_fd) {
                    // socket is main socket
                    debug("Got new connection on listener socket (fd=%d)\n", listener_fd);
                    int connect_fd = accept(listener_fd, (struct sockaddr *)&their_address, &addr_size);
                    if (connect_fd == -1) {
                        if (errno != EINTR) warn("%s\n", strerror(errno));
                        continue;
                    }
                    VLA_data new_connection = {
                        .fd_status.fd = connect_fd,
                        .fd_status.events = POLLIN,
                    };
                    VLA_insert(pfds_VLA, new_connection);
                } else {
                    // socket is not main socket
                    generic_packet *request = read_unknown_packet(pfds_item.fd);
                    shutdown(pfds_item.fd, SHUT_RD);

                    if (request->type == PROTO_CRUD) {
                        crud_packet *client_request = (crud_packet *)request->contents;
                        uint16_t hash_value = 0;
                        memcpy(&hash_value, client_request->key->contents, sizeof(uint16_t) > client_request->key->length ? client_request->key->length : sizeof(uint16_t));
                        hash_value = ntohs(hash_value);
                        client_info *new = malloc(sizeof(client_info));
                        if (new == NULL) {
                            panic("%s\n", strerror(errno));
                        }
                        new->key = hash_value;
                        new->fd = pfds_item.fd;
                        new->request = client_request;
                        debug("Storing client information with Key %#x, fd %d and request %p in internal Hash Table.\n", new->key, new->fd, new->request);
                        HASH_ADD_KEYPTR(hh, internal_hash_head, &new->key, sizeof(new->key), new);

                        if (peer_stores_hashvalue(&nodes[0], hash_value)) {
                            debug("I am responsible for the hash value, now sending back answer to Client.\n");
                            // Führe die Request vom Client aus und sende ihm zurück, ob das auch geklappt hat.
                            crud_packet *response = execute_ds_action(client_request);

                            send_crud_packet(pfds_item.fd, response);
                            shutdown(pfds_item.fd, SHUT_WR);
                            close(pfds_item.fd);

                            free_crud_packet(client_request);
                            free_crud_packet(response);
                            VLA_delete_by_index(pfds_VLA, i);
                        } else if (peer_stores_hashvalue(&nodes[2], hash_value)) {  // Nachfolger ist für den Bereich zuständig, einfach Request an ihn weiterleiten
                            debug("Successor is responsible for the hash value, now sending back answer to Client over one redirection.\n");
                            int peer_fd = establish_tcp_connection_from_ip4(nodes[2].node_ip, nodes[2].node_port);
                            send_crud_packet(peer_fd, client_request);
                            crud_packet *response = get_blank_crud_packet();
                            receive_crud_packet(peer_fd, response, READ_CONTROL);
                            send_crud_packet(pfds_item.fd, response);
                            close(pfds_item.fd);
                            free_crud_packet(client_request);
                            free_crud_packet(response);
                            VLA_delete_by_index(pfds_VLA, i);
                        } else {  // es ist noch nicht bekannt, wer für den Bereich verantwortlich ist -> lookup machen
                            debug("Don't know who is responsible for the hash value, starting lookup!\n");
                            chord_packet *pkg = get_blank_chord_packet();

                            pkg->action = LOOKUP;
                            pkg->hash_id = hash_value;
                            pkg->node_id = nodes[0].node_id;
                            pkg->node_ip = nodes[0].node_ip;
                            pkg->node_port = nodes[0].node_port;

                            int peer_fd = establish_tcp_connection_from_ip4(nodes[2].node_ip, nodes[2].node_port);
                            send_chord_packet(peer_fd, pkg);
                            close(peer_fd);
                            free(pkg);
                            VLA_delete_by_index(pfds_VLA, i);  // keine Ahnung ob das wirklich richtig ist
                        }
                    } else if (request->type == PROTO_CHORD) {
                        chord_packet *ring_message = (chord_packet *)request->contents;

                        if (ring_message->action == REPLY) {
                            debug("Got a reply, now I know who is responsible for the hash value. Trying to send answer to Client over one redirection.\n");
                            client_info *client = NULL;
                            HASH_FIND(hh, internal_hash_head, &ring_message->hash_id, sizeof(ring_message->hash_id), client);
                            if (client == NULL) {
                                warn("No client has sent a request with Key %#x. Something went wrong inside the ring or the client closed the connection.\n", ring_message->hash_id);
                                continue;
                                // TODO: bessere Fehlerbehebung, fds schließen etc
                            }

                            int peer_fd = establish_tcp_connection_from_ip4(ring_message->node_ip, ring_message->node_port);
                            send_crud_packet(peer_fd, client->request);
                            crud_packet *response = get_blank_crud_packet();
                            receive_crud_packet(peer_fd, response, READ_CONTROL);
                            VLA_delete_by_index(pfds_VLA, i);
                            send_crud_packet(client->fd, response);
                            HASH_DEL(internal_hash_head, client);
                            free_crud_packet(client->request);
                            free(client);
                        } else if (ring_message->action == LOOKUP) {
                            if (peer_stores_hashvalue(&nodes[2], ring_message->hash_id)) {
                                debug("Got a lookup request, my successor is responsible for the hash value. Sending back answer to the origin of the lookup.\n");
                                chord_packet *reply = get_blank_chord_packet();

                                reply->action = REPLY;
                                reply->hash_id = ring_message->hash_id;
                                reply->node_id = nodes[2].node_id;
                                reply->node_ip = nodes[2].node_ip;
                                reply->node_port = nodes[2].node_port;

                                int peer_fd = establish_tcp_connection_from_ip4(ring_message->node_ip, ring_message->node_port);
                                send_chord_packet(peer_fd, reply);
                                close(peer_fd);
                                VLA_delete_by_index(pfds_VLA, i);
                            } else {
                                debug("Got a lookup request, but I also don't know who is responsible for the hash value. Forwarding lookup to my successor.\n");
                                int peer_fd = establish_tcp_connection_from_ip4(nodes[2].node_ip, nodes[2].node_port);
                                send_chord_packet(peer_fd, ring_message);
                                close(peer_fd);
                                VLA_delete_by_index(pfds_VLA, i);
                            }
                        }

                        free(ring_message);
                    }
                }
            }
        }
    }

    close(listener_fd);
    ds_destruct();

    return EXIT_SUCCESS;
}