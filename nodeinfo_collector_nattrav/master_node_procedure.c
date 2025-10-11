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
    // ノード情報受信
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
        /*
        // メンバノードからノード登録要求を受信したら
        if(request_code == JOIN_REQUEST_CODE){
            receive_nodedata();
            add_nodedata_to_list();
            send_nodedata_list();   // メンバノードと中継サーバの両方にnodedata_listを送る。
        }
        // メンバノードからノード脱退要求を受信したら
        else if(request_code == LEAVE_REQUEST_CODE){
            receive_nodedata();
            remove_nodedata_from_list();
            send_nodedata_list();   // メンバノードと中継サーバの両方にnodedata_listを送る。
        }
        // 中継サーバから「データベースを受け取れ」要求を受信したら
        else if(request_code == READY_DB_CODE){
            receive_nodeinfo_database();
            send_nodeinfo_database();
        }*/
    }
    
    return 0;
};