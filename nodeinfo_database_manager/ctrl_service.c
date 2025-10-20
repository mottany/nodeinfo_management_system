#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "relay_protocol.h"
#include "ctrl_service.h"

int network_id = 1;

int accept_request() {
    static int sock = -1;
    static int inited = 0;

    if (!inited) {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            perror("[-]: socket(accept_join_request)");
            return -1;
        }
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(CTRL_MSG_PORT);
        if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("[-]: bind(accept_join_request)");
            close(sock);
            sock = -1;
            return -1;
        }
        fprintf(stderr, "[+]: Waiting for join request on UDP %d\n", CTRL_MSG_PORT);
        inited = 1;
    }

    char buf[256];
    struct sockaddr_in client;
    socklen_t clen = sizeof(client);
    ssize_t r = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&client, &clen);
    if (r < 0) {
        perror("[-]: recvfrom(accept_join_request)");
        return -1;
    }
    buf[r] = '\0';

    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client.sin_addr, ipstr, sizeof(ipstr));

    if (strcmp(buf, HELLO_RELAY_SERVER_MSG) == 0) {
        uint32_t nid_n = htonl((uint32_t)network_id);
        if (sendto(sock, &nid_n, sizeof(nid_n), 0, (struct sockaddr *)&client, clen) < 0) {
            perror("[-]: sendto(accept_join_request)");
            return -1;
        }
        fprintf(stderr, "[+]: HELLO from %s:%d -> assigned network_id=%d\n",
                ipstr, ntohs(client.sin_port), network_id);
        network_id++;
        return JOIN_REQUEST_CODE;
    } else if (strcmp(buf, READY_SEND_NODEDATA_LIST_MSG) == 0) {
        if (sendto(sock, READY_RECV_NODEDATA_LIST_MSG, strlen(READY_RECV_NODEDATA_LIST_MSG), 0,
                   (struct sockaddr *)&client, clen) < 0) {
            perror("[-]: sendto(READY_RECV_NODEDATA_LIST_MSG)");
            return -1;
        }
        fprintf(stderr, "[+]: READY_SEND from %s:%d -> replied READY_RECV\n",
                ipstr, ntohs(client.sin_port));
        return RECV_NODEDATA_LIST_REQUEST_CODE;
    } else {
        fprintf(stderr, "[-]: Unexpected message from %s:%d: '%s'\n",
                ipstr, ntohs(client.sin_port), buf);
        return -1;
    }
}
