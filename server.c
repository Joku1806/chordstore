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
#include "datastore.h"
#include "crud_protocol.h"

int is_running = 1;

// Wird ausgeführt, wenn das Programm ein SIGINT Signal bekommt.
// Diese Funktion setzt is_running auf false, damit nach dem while-loop Handling gemacht werden kann
void close_handler(int num) {
    is_running = 0;
}

// erstellt eine Verbindungssocket
int connect_to_client(struct addrinfo *address_list) {
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

    freeaddrinfo(address_list);
    return socket_fd;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Benutzung: %s <Port>\n", argv[0]);
        exit(EXIT_FAILURE);
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

    int socket_fd = connect_to_client(address_list);
    if (socket_fd == -1) {
        fprintf(stderr, "Konnte keine Socket erstellen.\n");
        exit(EXIT_FAILURE);
    }

    if (listen(socket_fd, 1) == -1) {
        fprintf(stderr, "listen(): %s\n", strerror(errno));
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

        int connect_fd = accept(socket_fd, (struct sockaddr *)&their_address, &addr_size);
        if (connect_fd == -1) {
            if (errno != EINTR) fprintf(stderr, "accept(): %s\n", strerror(errno));
            continue;
        }

        // Erhalte Request vom Client und informiere ihn danach darüber, dass man fertig gelesen hat.
        // Das ist hier zwar nicht so kritisch wie im umgekehrten Fall, weil der Client ja weiß wieviel er schreiben muss,
        // ist aber trotzdem gute Praxis.
        hash_packet *request = receive_hash_packet(connect_fd);
        if (request == NULL) {
            fprintf(stderr, "Error while parsing client request.\n");
            close(connect_fd);
            continue;
        }
        shutdown(connect_fd, SHUT_RD);

        // Führe die Request vom Client aus und sende ihm zurück, ob das auch geklappt hat.
        hash_packet *response = execute_ds_action(request);

        send_hash_packet(connect_fd, response);
        shutdown(connect_fd, SHUT_WR);
        close(connect_fd);

        free_hash_packet(request);
        free_hash_packet(response);
    }

    close(socket_fd);
    ds_destruct();

    return EXIT_SUCCESS;
}