#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>

#include "common.h"
#include "master_node_procedure.h"
#include "sock_wrapper_functions.h"

enum {
    JOIN_REQUEST_CODE = 1,
    LEAVE_REQUEST_CODE = 2,
    RECV_DB_REQUEST_CODE = 3
};

static const char *RELAY_SERVER_IP = "127.0.0.1";
static const int  RELAY_SERVER_PORT = 9000;

static int accept_request() {
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
    fprintf(stderr, "[+]: Start receiving nodedata from member node\n");

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

    fprintf(stderr, "[+]: Successfully received nodedata from %s (uid=%d, cpu=%d)\n",
            inet_ntoa(client_addr.sin_addr), out->userid, out->cpu_core_num);

    close(conn_sock);
    close(listen_sock);
    return 0;
}

static int resize_nodedata_list(struct nodedata_list *list) {
    fprintf(stderr, "[+]: Start resizing nodedata_list from max size %d to %d\n",
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

static int remove_nodedata_from_list() {
    // 受信したノード情報をリストから削除
}

static int distribute_nodedata_list(const struct nodedata_list *list) {
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
    // 中継サーバにクラスタ参加要求
}

static int send_nodeinfo_database() {
    // メンバノードにノード情報データベース送信
}

int run_master_node_procedure() {
    fprintf(stderr, "[+]: Start master node procedure\n");

    // nodedata_listの定義と初期化
    struct nodedata_list nd_list;
    memset(&nd_list, 0, sizeof(nd_list));
    nd_list.max_size = INITIAL_SIZE_OF_NODEDATA_LIST;

    while(1){
        // if(request_join_huge_cluster() < 0){
        //     fprintf(stderr, "[-]: Failed to request join to huge cluster\n");
        //     return -1;
        // }
        // fprintf(stderr, "[+]: Successfully requested join to huge cluster\n");
        // メンバノードと中継サーバから要求メッセージを受け入れる
        
        int request_code = accept_request();
        
        // メンバノードからノード登録要求を受信したら
        if(request_code == JOIN_REQUEST_CODE){
            struct nodedata nd;
            memset(&nd, 0, sizeof(nd));
            if(receive_nodedata(&nd) < 0){
                 fprintf(stderr, "[-]: Failed to receive nodedata from member node\n");
                 return -1;
            }
            if(add_nodedata_to_list(&nd, &nd_list) < 0){
                fprintf(stderr, "[-]: Failed to add nodedata to list\n");
                return -1;
            }
            fprintf(stderr, "[+]: Added nodedata to list (current size: %d)\n", nd_list.current_size);
            if(distribute_nodedata_list(&nd_list) < 0){   // メンバノードと中継サーバの両方にnodedata_listを送る。
                fprintf(stderr, "[-]: Failed to send nodedata_list\n");
                return -1;
            }
            // update_nodeinfo();
            // update_hostfile();
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