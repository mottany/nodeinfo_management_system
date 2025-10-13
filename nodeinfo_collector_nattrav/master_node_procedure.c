#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>

#include "common_asset.h"
#include "master_node_procedure.h"

int accept_request() {
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

int receive_nodedata() {
    fprintf(stderr, "[+]: Start receiving nodedata from member node\n");

    int listen_sock, conn_sock;
    struct sockaddr_in addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct nodedata nd;
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
    size_t need = sizeof(nd);
    size_t got = 0;
    while (got < need) {
        int r = wrapped_recv(conn_sock, ((char *)&nd) + got, need - got, 0);
        if (r <= 0) {
            close(conn_sock);
            close(listen_sock);
            return -1;
        }
        got += (size_t)r;
    }

    fprintf(stderr, "[+]: Successfully received nodedata from %s (uid=%d, cpu=%d)\n",
            inet_ntoa(client_addr.sin_addr), nd.userid, nd.cpu_core_num);

    close(conn_sock);
    close(listen_sock);
    return 0;
}

int add_nodedata_to_list() {
    // 受信したノード情報をリストに追加
}

int remove_nodedata_from_list() {
    // 受信したノード情報をリストから削除
}

int send_nodedata_list() {
    // ノード情報リスト送信
}

int request_join_huge_cluster() {
    // 中継サーバにクラスタ参加要求
}

int receive_nodeinfo_database() {
    // 中継サーバからノード情報データベース受信
}

int send_nodeinfo_database() {
    // メンバノードにノード情報データベース送信
}

int run_master_node_procedure() {
    fprintf(stderr, "[+]: Start master node procedure\n");

    while(1){
        // メンバノードと中継サーバから要求メッセージを受け入れる
        int request_code = accept_request();
        
        // メンバノードからノード登録要求を受信したら
        if(request_code == JOIN_REQUEST_CODE){
            if(receive_nodedata() < 0){
                fprintf(stderr, "[-]: Failed to receive nodedata from member node\n");
                return -1;
            }
            // add_nodedata_to_list();
            // send_nodedata_list();   // メンバノードと中継サーバの両方にnodedata_listを送る。
        }
        // メンバノードからノード脱退要求を受信したら
        /*else if(request_code == LEAVE_REQUEST_CODE){
            receive_nodedata();
            remove_nodedata_from_list();
            send_nodedata_list();   // メンバノードと中継サーバの両方にnodedata_listを送る。
        }
        // 中継サーバから「データベースを受け取れ」要求を受信したら
        else if(request_code == READY_DB_REQUEST_CODE){
            receive_nodeinfo_database();
            send_nodeinfo_database();
        }*/
    }
    
    return 0;
};