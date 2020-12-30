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

// erstellt eine Verbindungssocket
int setup_tcp_listener(struct addrinfo *address_list) {
    int socket_fd = -1;

    for (struct addrinfo *entry = address_list; entry != NULL; entry = entry->ai_next) {
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

    if (socket_fd != -1 && listen(socket_fd, 1) == -1) {
        close(socket_fd);
        fprintf(stderr, "listen(): %s\n", strerror(errno));
        return -1;
    }

    freeaddrinfo(address_list);
    return socket_fd;
}

int string_to_uint16(char *src, uint16_t *dest) {
    char *parse_stop;
    errno = 0;
    long converted = strtol(src, &parse_stop, 10);

    if (errno || parse_stop == src || *parse_stop != '\0' || converted < 0 || converted >= 0x10000) return 0;
    *dest = (uint16_t)converted;
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 10) {
        fprintf(stderr, "Benutzung: %s <ID self> <Host self> <Port self>\n\t<ID prev> <Host prev> <Port prev>\n\t<ID next> <Host next> <Port next>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    peer nodes[3];

    for (int i = 0; i < 9; i += 3) {
        if (!string_to_uint16(argv[i], &nodes[i / 3].node_id)) {
            fprintf(stderr, "Error converting node ID.\n");
            exit(EXIT_FAILURE);
        }
        if (!string_to_uint16(argv[i + 2], &nodes[i / 3].node_port)) {
            fprintf(stderr, "Error converting node port.\n");
            exit(EXIT_FAILURE);
        }

        struct addrinfo peer_hints, *peer_address_list;
        memset(&peer_hints, 0, sizeof peer_hints);
        peer_hints.ai_family = AF_INET;        // nur über IPv4
        peer_hints.ai_socktype = SOCK_STREAM;  // rede über TCP mit Server

        int info_success = getaddrinfo(argv[i + 1], argv[i + 2], &peer_hints, &peer_address_list);
        if (info_success != 0) {
            fprintf(stderr, "getaddrinfo(): %s", gai_strerror(info_success));
            return -1;
        }

        nodes[i / 3].node_ip = ((struct sockaddr_in *)peer_address_list->ai_addr)->sin_addr.s_addr;
    }

    struct addrinfo hints, *address_list;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;      // egal ob IPv4 oder IPv6
    hints.ai_socktype = SOCK_STREAM;  // rede über TCP mit Client
    hints.ai_flags = AI_PASSIVE;      // fülle automatisch mit lokaler IP aus

    int info_success = getaddrinfo(NULL, argv[1], &hints, &address_list);
    if (info_success != 0) {
        fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(info_success));
        exit(EXIT_FAILURE);
    }

    int listener_fd = setup_tcp_listener(address_list);
    if (listener_fd == -1) {
        fprintf(stderr, "Konnte keine Socket erstellen.\n");
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

<<<<<<< HEAD
        generic_packet *request = read_unknown_packet(connect_fd);
=======
        // Erhalte Request vom Client und informiere ihn danach darüber, dass man fertig gelesen hat.
        // Das ist hier zwar nicht so kritisch wie im umgekehrten Fall, weil der Client ja weiß wieviel er schreiben muss,
        // ist aber trotzdem gute Praxis.
        hash_packet *request = receive_hash_packet(connect_fd);
>>>>>>> 2c4620ce91d456fe0e400b43fdf385a70a59ac88
        if (request == NULL) {
            fprintf(stderr, "Error while parsing client request.\n");
            close(connect_fd);
            continue;
        }
        shutdown(connect_fd, SHUT_RD);

<<<<<<< HEAD
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
=======
        // Führe die Request vom Client aus und sende ihm zurück, ob das auch geklappt hat.
        hash_packet *response = execute_ds_action(request);

        send_hash_packet(connect_fd, response);
        shutdown(connect_fd, SHUT_WR);
        close(connect_fd);

        free_hash_packet(request);
        free_hash_packet(response);
>>>>>>> 2c4620ce91d456fe0e400b43fdf385a70a59ac88
    }

    close(listener_fd);
    ds_destruct();

    return EXIT_SUCCESS;
}