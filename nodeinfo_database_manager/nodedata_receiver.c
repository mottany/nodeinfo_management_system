#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "relay_protocol.h"
#include "db_types.h"
#include "nodedata_receiver.h"

struct nodedata_list *receive_nodedata_list(void) {
    static int sock = -1;
    static int inited = 0;

    if (!inited) {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            perror("[-]: socket(receive_nodedata_list)");
            return NULL;
        }
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(NODEDATA_LIST_PORT);
        if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("[-]: bind(receive_nodedata_list)");
            close(sock);
            sock = -1;
            return NULL;
        }
        fprintf(stderr, "[+]: Waiting for nodedata_list on UDP %d\n", NODEDATA_LIST_PORT);
        inited = 1;
    }

    enum { MAX_UDP_PAYLOAD = 65535 };
    char *buf = (char *)malloc(MAX_UDP_PAYLOAD);
    if (!buf) {
        fprintf(stderr, "[-]: malloc failed in receive_nodedata_list\n");
        return NULL;
    }

    struct sockaddr_in sender;
    socklen_t slen = sizeof(sender);
    ssize_t r = recvfrom(sock, buf, MAX_UDP_PAYLOAD, 0, (struct sockaddr *)&sender, &slen);
    if (r < 0) {
        perror("[-]: recvfrom(receive_nodedata_list)");
        free(buf);
        return NULL;
    }

    if (r < (ssize_t)sizeof(struct nodedata_list)) {
        fprintf(stderr, "[-]: too short nodedata_list datagram: %zd bytes\n", r);
        free(buf);
        return NULL;
    }

    struct nodedata_list *list = (struct nodedata_list *)buf;
    size_t expected = sizeof(struct nodedata_list) + sizeof(struct nodedata) * (size_t)list->current_size;
    if ((size_t)r < expected) {
        fprintf(stderr, "[-]: corrupted nodedata_list (recv=%zd, expected>=%zu, count=%d)\n",
                r, expected, list->current_size);
        free(buf);
        return NULL;
    }

    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sender.sin_addr, ipstr, sizeof(ipstr));
    fprintf(stderr, "[+]: Received nodedata_list (count=%d) from %s:%d\n",
            list->current_size, ipstr, ntohs(sender.sin_port));

    char *shrink = realloc(buf, (size_t)r);
    if (shrink) buf = shrink;
    return (struct nodedata_list *)buf;
}

void print_nodedata_list(const struct nodedata_list *list) {
    if (!list) {
        fprintf(stderr, "[-]: print_nodedata_list(): list is NULL\n");
        return;
    }
    printf("Nodedata List (current size: %d, max size: %d):\n",
           list->current_size, list->max_size);
    for (int i = 0; i < list->current_size; i++) {
        struct nodedata *nd = &list->nodedatas[i];
        printf("  Node %d: IP=%d, UserID=%d, CPU Cores=%d\n",
               i, nd->ipaddress, nd->userid, nd->cpu_core_num);
    }
    return;
}