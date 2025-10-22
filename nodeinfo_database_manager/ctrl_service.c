#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "relay_protocol.h"
#include "ctrl_service.h"

// Keep control socket and last requester to reply on the same 5-tuple (NAT friendly)
static int g_ctrl_sock = -1;
static int g_ctrl_inited = 0;
static struct sockaddr_in g_last_client;
static socklen_t g_last_client_len = 0;

int network_id = 1;

int accept_request() {
    if (!g_ctrl_inited) {
        g_ctrl_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (g_ctrl_sock < 0) {
            perror("[-]: socket(accept_join_request)");
            return -1;
        }
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(CTRL_MSG_PORT);
        if (bind(g_ctrl_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("[-]: bind(accept_join_request)");
            close(g_ctrl_sock);
            g_ctrl_sock = -1;
            return -1;
        }
        fprintf(stderr, "[+]: Waiting for join request on UDP %d\n", CTRL_MSG_PORT);
        g_ctrl_inited = 1;
    }

    char buf[256];
    struct sockaddr_in client;
    socklen_t clen = sizeof(client);
    ssize_t r = recvfrom(g_ctrl_sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&client, &clen);
    if (r < 0) {
        perror("[-]: recvfrom(accept_join_request)");
        return -1;
    }
    buf[r] = '\0';

    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client.sin_addr, ipstr, sizeof(ipstr));

    if (strcmp(buf, HELLO_RELAY_SERVER_MSG) == 0) {
        uint32_t nid_n = htonl((uint32_t)network_id);
        if (sendto(g_ctrl_sock, &nid_n, sizeof(nid_n), 0, (struct sockaddr *)&client, clen) < 0) {
            perror("[-]: sendto(accept_join_request)");
            return -1;
        }
        fprintf(stderr, "[+]: HELLO from %s:%d -> assigned network_id=%d\n",
                ipstr, ntohs(client.sin_port), network_id);
        network_id++;
        return JOIN_REQUEST_CODE;
    } else if (strcmp(buf, READY_SEND_NODEDATA_LIST_MSG) == 0) {
        if (sendto(g_ctrl_sock, READY_RECV_NODEDATA_LIST_MSG, strlen(READY_RECV_NODEDATA_LIST_MSG), 0,
                   (struct sockaddr *)&client, clen) < 0) {
            perror("[-]: sendto(READY_RECV_NODEDATA_LIST_MSG)");
            return -1;
        }
        fprintf(stderr, "[+]: READY_SEND from %s:%d -> replied READY_RECV\n",
                ipstr, ntohs(client.sin_port));
        return RECV_NODEDATA_LIST_REQUEST_CODE;
    } else if (strcmp(buf, GIVE_ME_DB_MSG) == 0) {
        // remember the requester to reply later
        g_last_client = client;
        g_last_client_len = clen;
        fprintf(stderr, "[+]: GIVE_ME_DB from %s:%d\n",
                ipstr, ntohs(client.sin_port));
        return NODEINFO_DB_REQUEST_CODE;
    } else {
        fprintf(stderr, "[-]: Unexpected message from %s:%d: '%s'\n",
                ipstr, ntohs(client.sin_port), buf);
        return -1;
    }
}

int send_nodeinfo_database(const struct nodeinfo_database *db) {
    if (g_ctrl_sock < 0 || !g_ctrl_inited || g_last_client_len == 0) {
        fprintf(stderr, "[-]: send_nodeinfo_database: no requester context\n");
        return -1;
    }

    // If db is NULL, send an empty header-only database (current_size=0)
    struct nodeinfo_database empty_hdr;
    const struct nodeinfo_database *payload = db ? db : &empty_hdr;
    if (!db) {
        empty_hdr.max_size = 0;
        empty_hdr.current_size = 0;
    }

    size_t bytes = sizeof(struct nodeinfo_database)
                 + sizeof(struct nodeinfo_database_element) * (size_t)payload->current_size;
    ssize_t s = sendto(g_ctrl_sock, payload, bytes, 0,
                       (struct sockaddr *)&g_last_client, g_last_client_len);
    if (s < 0 || (size_t)s != bytes) {
        perror("[-]: sendto(nodeinfo_database)");
        return -1;
    }

    fprintf(stderr, "[+]: Sent nodeinfo_database (elements=%d, bytes=%zu) to %s:%d\n",
            payload->current_size, bytes,
            inet_ntoa(g_last_client.sin_addr), ntohs(g_last_client.sin_port));
    return 0;
}
