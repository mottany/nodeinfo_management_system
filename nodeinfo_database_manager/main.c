#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define HELLO_RELAY_SERVER_MSG "Hello_relay_server!"
#define Welcome_CLUSTER_MSG "Welcome_cluster!"

#define CTRL_MSG_PORT 8000
#define NODEDATA_LIST_PORT 8002

int network_id = 1;

struct nodedata {
    int ipaddress;
    int userid;
    int cpu_core_num;
};

struct nodedata_list {
    int max_size;
    int current_size;
    struct nodedata nodedatas[];
};

struct nodeinfo_database_element {
    int network_id;
    uint32_t ipaddress;
    int userid;
    int control_port_num;
    int message_port_num;
    int cpu_core_num;
};

struct nodeinfo_database {
    int max_size;
    int current_size;
    struct nodeinfo_database_element elements[];
};

int accept_join_request() {
    // クラスタ参加要求受信（UDP、ソケットは一度だけ初期化）
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

    if (strcmp(buf, HELLO_RELAY_SERVER_MSG) != 0) {
        fprintf(stderr, "[-]: Unexpected message from %s:%d: '%s'\n",
                ipstr, ntohs(client.sin_port), buf);
        return -1;
    }

    uint32_t nid_n = htonl((uint32_t)network_id);
    if (sendto(sock, &nid_n, sizeof(nid_n), 0, (struct sockaddr *)&client, clen) < 0) {
        perror("[-]: sendto(accept_join_request)");
        return -1;
    }

    fprintf(stderr, "[+]: Assigned network_id=%d to %s:%d\n",
            network_id, ipstr, ntohs(client.sin_port));
    network_id++;
    return 0;
}

struct nodedata_list *receive_nodedata_list(void) {
    // ノード情報リスト受信（ソケットは一度だけ作成・bind、以後ブロッキングで待機）
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

    for (int i = 0; i < list->current_size; i++) {
        struct in_addr ia = { .s_addr = (uint32_t)list->nodedatas[i].ipaddress };
        char nip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ia, nip, sizeof(nip));
        fprintf(stderr, "  Node %d: IP=%s, UID=%d, CPU=%d\n",
                i, nip, list->nodedatas[i].userid, list->nodedatas[i].cpu_core_num);
    }

    char *shrink = realloc(buf, (size_t)r);
    if (shrink) buf = shrink;
    return (struct nodedata_list *)buf;
}

// 必要ポート（node 側の既定値）
#ifndef CTRL_MSG_PORT
#define CTRL_MSG_PORT 8000
#endif
#ifndef NODEDATA_PORT
#define NODEDATA_PORT 8001
#endif

struct nodeinfo_database *update_nodeinfo_database(const struct nodedata_list *list) {
    if (!list) {
        fprintf(stderr, "[-]: update_nodeinfo_database: list is NULL\n");
        return NULL;
    }

    int count = list->current_size;
    if (count < 0) {
        fprintf(stderr, "[-]: update_nodeinfo_database: negative count %d\n", count);
        return NULL;
    }

    size_t sz = sizeof(struct nodeinfo_database) + sizeof(struct nodeinfo_database_element) * (size_t)count;
    struct nodeinfo_database *db = (struct nodeinfo_database *)malloc(sz);
    if (!db) {
        fprintf(stderr, "[-]: malloc failed in update_nodeinfo_database (size=%zu)\n", sz);
        return NULL;
    }

    db->max_size = count;
    db->current_size = count;
    for (int i = 0; i < count; i++) {
        db->elements[i].network_id = -1; // ここでは不明。必要なら accept_join_request での割り当てを紐づける
        db->elements[i].ipaddress = (uint32_t)list->nodedatas[i].ipaddress;
        db->elements[i].userid = list->nodedatas[i].userid;
        db->elements[i].control_port_num = CTRL_MSG_PORT;
        db->elements[i].message_port_num = NODEDATA_PORT;
        db->elements[i].cpu_core_num = list->nodedatas[i].cpu_core_num;
    }

    fprintf(stderr, "[+]: Built nodeinfo_database (count=%d)\n", count);
    return db;
}

int send_nodeinfo_database() {
    // TODO: 必要に応じてDBを配布/送付する処理
    return 0;
}

int main(){
    fprintf(stderr, "[+]: Start nodeinfo database manager\n");

    while(1){
        if(accept_join_request() < 0){
            fprintf(stderr, "[-]: Error in accepting join request\n");
            continue;
        }
        /*struct nodedata_list *list = receive_nodedata_list();
        if(!list){
            fprintf(stderr, "[-]: Error in receiving nodedata list\n");
            continue;
        }
        struct nodeinfo_database *db = update_nodeinfo_database(list);
        free(list);
        if(!db){
            fprintf(stderr, "[-]: Error in updating nodeinfo database\n");
            continue;
        }
        if(send_nodeinfo_database() < 0){
            fprintf(stderr, "[-]: Error in sending nodeinfo database\n");
            free(db);
            continue;
        }
        free(db);*/
    }

    return 0;
}