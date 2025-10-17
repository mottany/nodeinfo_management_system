#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>

#include "common.h"
#include "master_node_procedure.h"
#include "sock_wrapper_functions.h"

enum {
    JOIN_REQUEST_CODE = 1,
    LEAVE_REQUEST_CODE = 2,
    RECV_DB_REQUEST_CODE = 3
};

static const char *HELLO_RELAY_SERVER_MSG = "Hello_relay_server!";

static const char *RELAY_SERVER_IP = "160.12.172.77";

static struct nodedata_list* create_nodedata_list(void) {
    fprintf(stderr, "[+]: Creating nodedata_list\n");
    int cap = (INITIAL_SIZE_OF_NODEDATA_LIST > 0) ? INITIAL_SIZE_OF_NODEDATA_LIST : 1;
    size_t bytes = sizeof(struct nodedata_list) + sizeof(struct nodedata) * (size_t)cap;
    struct nodedata_list *list = (struct nodedata_list *)malloc(bytes);
    if (!list) {
        fprintf(stderr, "[-]: Failed to allocate nodedata_list\n");
        return NULL;
    }
    list->max_size = cap;
    list->current_size = 0;

    // 自ノード情報を取得して先頭に格納
    struct nodedata me = get_my_nodedata();
    if (me.ipaddress == 0 || /*me.userid < 0 ||*/ me.cpu_core_num <= 0) {
        fprintf(stderr, "[-]: get_my_nodedata() returned invalid data (ip=%u uid=%d cpu=%d)\n",
                me.ipaddress, me.userid, me.cpu_core_num);
        free(list);
        return NULL;
    }
    list->nodedatas[list->current_size++] = me;

    // 出力（IPは文字列化）
    char ipstr[INET_ADDRSTRLEN];
    struct in_addr ia = { .s_addr = me.ipaddress };
    inet_ntop(AF_INET, &ia, ipstr, sizeof(ipstr));
    fprintf(stderr, "[+]: Initialized nodedata_list with self: IP=%s, UID=%d, CPU=%d (max=%d)\n",
            ipstr, me.userid, me.cpu_core_num, list->max_size);
    return list;
}

static int accept_request() {
    fprintf(stderr, "[+]: Waiting for control message from member nodes or relay server\n");

    int sock;
    struct sockaddr_in addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buf[256];

    /* UDP ソケット作成 */
    sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    /* バインド */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(CTRL_MSG_PORT);
    if (wrapped_bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    /* メッセージ受信 */
    int r = wrapped_recvfrom(sock, buf, sizeof(buf) - 1, 0,
                             (struct sockaddr *)&client_addr, &client_len);
    if (r <= 0) {
        close(sock);
        return -1;
    }
    buf[r] = '\0';

    /* リクエスト別処理 */
    if (strcmp(buf, HELLO_CLUSTER_MSG) == 0) {
        if (sendto(sock, WELCOME_NODE_MSG, strlen(WELCOME_NODE_MSG), 0,
                   (struct sockaddr *)&client_addr, client_len) < 0) {
            perror("[-]: Failed to send welcome message");
            close(sock);
            return -1;
        }
        fprintf(stderr, "[+]: Accepted join from %s:%d\n",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        close(sock);
        return JOIN_REQUEST_CODE;
    } else if (strcmp(buf, BYE_CLUSTER_MSG) == 0) {
        if (sendto(sock, BYE_NODE_MSG, strlen(BYE_NODE_MSG), 0,
                   (struct sockaddr *)&client_addr, client_len) < 0) {
            perror("[-]: Failed to send bye message");
            close(sock);
            return -1;
        }
        fprintf(stderr, "[+]: Accepted leave from %s:%d\n",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        close(sock);
        return LEAVE_REQUEST_CODE;
    } else if (strcmp(buf, READY_SEND_DB_MSG) == 0) {
        if (sendto(sock, READY_RECV_DB_MSG, strlen(READY_RECV_DB_MSG), 0,
                   (struct sockaddr *)&client_addr, client_len) < 0) {
            perror("[-]: Failed to send ready-recv-db message");
            close(sock);
            return -1;
        }
        fprintf(stderr, "[+]: Relay server ready request from %s:%d\n",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        close(sock);
        return RECV_DB_REQUEST_CODE;
    } else {
        fprintf(stderr, "[-]: Unknown control message: %s\n", buf);
        close(sock);
        return -1;
    }
}

static int receive_nodedata(struct nodedata *out) {
    fprintf(stderr, "[+]: Receiving nodedata from member node\n");

    if (!out) {
        fprintf(stderr, "[-]: receive_nodedata(): out is NULL\n");
        return -1;
    }

    int listen_sock, conn_sock;
    struct sockaddr_in addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int opt = 1;

    listen_sock = wrapped_socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        return -1;
    }

    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(NODEDATA_PORT);

    if (wrapped_bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(listen_sock);
        return -1;
    }

    if (wrapped_listen(listen_sock, 1) < 0) {
        close(listen_sock);
        return -1;
    }

    conn_sock = wrapped_accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
    if (conn_sock < 0) {
        close(listen_sock);
        return -1;
    }

    // 構造体サイズぶんを受信（短い受信に備えてループ）
    size_t need = sizeof(*out);
    size_t got = 0;
    while (got < need) {
        int r = wrapped_recv(conn_sock, ((char *)out) + got, need - got, 0);
        if (r <= 0) {
            close(conn_sock);
            close(listen_sock);
            return -1;
        }
        got += (size_t)r;
    }

    close(conn_sock);
    close(listen_sock);
    return 0;
}

static int resize_nodedata_list(struct nodedata_list *list) {
    fprintf(stderr, "[+]: Resizing nodedata_list from max size %d to %d\n",
            list->max_size, list->max_size * 2);
    if (!list) {
        fprintf(stderr, "[-]: resize_nodedata_list(): list is NULL\n");
        return -1;
    }
    int new_max_size = list->max_size * 2;
    struct nodedata_list *new_list = malloc(sizeof(struct nodedata_list) +
                                           sizeof(struct nodedata) * new_max_size);
    if (!new_list) {
        fprintf(stderr, "[-]: Failed to allocate memory for resized nodedata_list\n");
        return -1;
    }
    new_list->max_size = new_max_size;
    new_list->current_size = list->current_size;
    memcpy(new_list->nodedatas, list->nodedatas,
           sizeof(struct nodedata) * list->current_size);
    free(list);
    list = new_list;
    fprintf(stderr, "[+]: Successfully resized nodedata_list to new max size %d\n", list->max_size);
    return 0;
}

static int add_nodedata_to_list(const struct nodedata *nd, struct nodedata_list *list) {
    fprintf(stderr, "[+]: Adding nodedata to nodedata_list\n");
    if (!nd || !list) {
        fprintf(stderr, "[-]: add_nodedata_to_list(): invalid args\n");
        return -1;
    }
    if (list->current_size >= list->max_size) {
        fprintf(stderr, "[-]: nodedata_list is full (current=%d, max=%d)\n",
                list->current_size, list->max_size);
        if(resize_nodedata_list(list) < 0) {
            return -1;
        }
    }
    list->nodedatas[list->current_size] = *nd;
    list->current_size++;
    return 0;
}

static int remove_nodedata_from_list(struct nodedata_list *list, int index) {
    fprintf(stderr, "[+]: Removing nodedata from nodedata_list\n");
    if (!list || index < 0 || index >= list->current_size) {
        fprintf(stderr, "[-]: remove_nodedata_from_list(): invalid args\n");
        return -1;
    }
    // 削除対象のノード情報を上書き
    for (int i = index; i < list->current_size - 1; i++) {
        list->nodedatas[i] = list->nodedatas[i + 1];
    }
    list->current_size--;
    fprintf(stderr, "[+]: Successfully removed nodedata from list (new size: %d)\n", list->current_size);
    return 0;
}

static int distribute_nodedata_list(const struct nodedata_list *list) {
    fprintf(stderr, "[+]: Distributing nodedata_list to member nodes and relay server\n");
    if (!list) {
        fprintf(stderr, "[-]: distribute_nodedata_list(): list is NULL\n");
        return -1;
    }
    if (list->current_size < 0 || list->max_size <= 0) {
        fprintf(stderr, "[-]: distribute_nodedata_list(): invalid list sizes\n");
        return -1;
    }

    // 送信サイズはヘッダ + 現在要素数ぶんの可変配列
    size_t payload_len = sizeof(struct nodedata_list) +
                         sizeof(struct nodedata) * (size_t)list->current_size;

    int sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    int failures = 0;
    for (int i = 0; i < list->current_size; i++) {
        uint32_t ip = list->nodedatas[i].ipaddress; // network byte order (uint32_t)
        if (ip == 0) continue; // 不正IPはスキップ

        struct sockaddr_in dst;
        memset(&dst, 0, sizeof(dst));
        dst.sin_family = AF_INET;
        dst.sin_port = htons(NODEDATA_LIST_PORT);
        dst.sin_addr.s_addr = ip;

        ssize_t n = sendto(sock, list, payload_len, 0,
                           (struct sockaddr *)&dst, sizeof(dst));
        if (n < 0 || (size_t)n != payload_len) {
            perror("[-]: sendto(nodedata_list)");
            failures++;
            continue;
        }

        char ipstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &dst.sin_addr, ipstr, sizeof(ipstr));
        fprintf(stderr, "[+]: Sent nodedata_list (count=%d) to %s:%d\n",
                list->current_size, ipstr, NODEDATA_LIST_PORT);
    }

    close(sock);
    return failures ? -1 : 0;
}

static int request_join_huge_cluster() {
    // 中継サーバにクラスタ参加要求: HELLO を送り、network_id(uint32_t)を受け取る
    fprintf(stderr, "[+]: Requesting to join huge cluster via relay server\n");

    int sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    struct sockaddr_in relay;
    memset(&relay, 0, sizeof(relay));
    relay.sin_family = AF_INET;
    relay.sin_port = htons(CTRL_MSG_PORT);
    if (inet_pton(AF_INET, RELAY_SERVER_IP, &relay.sin_addr) != 1) {
        fprintf(stderr, "[-]: inet_pton failed for RELAY_SERVER_IP=%s\n", RELAY_SERVER_IP);
        close(sock);
        return -1;
    }

    size_t msglen = strlen(HELLO_RELAY_SERVER_MSG);
    if (sendto(sock, HELLO_RELAY_SERVER_MSG, msglen, 0,
               (struct sockaddr *)&relay, sizeof(relay)) < 0) {
        perror("[-]: sendto(HELLO_RELAY_SERVER_MSG)");
        close(sock);
        return -1;
    }

    uint32_t nid_n = 0;
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    int r = wrapped_recvfrom(sock, &nid_n, sizeof(nid_n), 0,
                              (struct sockaddr *)&from, &fromlen);
    if (r <= 0) {
        close(sock);
        return -1;
    }
    if (r != (int)sizeof(nid_n)) {
        fprintf(stderr, "[-]: unexpected reply size: %d (expected %zu)\n", r, sizeof(nid_n));
        close(sock);
        return -1;
    }

    int network_id = (int)ntohl(nid_n);

    char rip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from.sin_addr, rip, sizeof(rip));
    fprintf(stderr, "[+]: Relay server assigned network_id=%d (from %s:%d)\n",
            network_id, rip, ntohs(from.sin_port));

    close(sock);
    return network_id;
}

static int send_nodeinfo_database() {
    // メンバノードにノード情報データベース送信
}

int run_master_node_procedure() {
    fprintf(stderr, "[+]: Start master node procedure\n");

    // nodedata_listの作成（自ノード情報を格納）
    struct nodedata_list *nd_list = create_nodedata_list();
    if (!nd_list) {
        fprintf(stderr, "[-]: Failed to create nodedata_list\n");
        return -1;
    }

    if (request_join_huge_cluster() < 0) {
        fprintf(stderr, "[-]: Failed to join huge cluster via relay server\n");
        return -1;
    }

    while(1){
        int request_code = accept_request();
        
        // メンバノードからノード登録要求を受信したら
        if(request_code == JOIN_REQUEST_CODE){
            struct nodedata nd;
            memset(&nd, 0, sizeof(nd));
            if(receive_nodedata(&nd) < 0){
                 fprintf(stderr, "[-]: Failed to receive nodedata from member node\n");
                 return -1;
            }
            fprintf(stderr, "[+]: Successfully received nodedata from %s (uid=%d, cpu=%d)\n",
                    inet_ntoa(*(struct in_addr *)&nd.ipaddress), nd.userid, nd.cpu_core_num);
            if(add_nodedata_to_list(&nd, nd_list) < 0){
                fprintf(stderr, "[-]: Failed to add nodedata to list\n");
                return -1;
            }
            fprintf(stderr, "[+]: Successfully added nodedata to list (current size: %d)\n", nd_list->current_size);
            if (print_nodedata_list(nd_list) < 0) {
                fprintf(stderr, "[-]: Failed to print nodedata list\n");
                return -1;
            }
            if(distribute_nodedata_list(nd_list) < 0){   // メンバノードと中継サーバの両方にnodedata_listを送る。
                fprintf(stderr, "[-]: Failed to send nodedata_list\n");
                return -1;
            }
            fprintf(stderr, "[+]: Successfully sent nodedata_list to all member nodes and relay server\n");
            if(update_nodeinfo(nd_list) < 0) {
                fprintf(stderr, "[-]: Failed to update nodeinfo\n");
                return -1;
            }
            if(update_hostfile(nd_list) < 0) {
                fprintf(stderr, "[-]: Failed to update hostfile\n");
                return -1;
            }
            fprintf(stderr, "[+]: Successfully updated nodeinfo and hostfile\n");
        }
        // メンバノードからノード脱退要求を受信したら
        /*else if(request_code == LEAVE_REQUEST_CODE){
            receive_nodedata();
            remove_nodedata_from_list();
            distribute_nodedata_list();   // メンバノードと中継サーバの両方にnodedata_listを送る。
        }
        // 中継サーバから「データベースを受け取れ」要求を受信したら
        else if(request_code == READY_DB_REQUEST_CODE){
            receive_nodeinfo_database();
            send_nodeinfo_database();

        }*/
    }
    
    return 0;
};