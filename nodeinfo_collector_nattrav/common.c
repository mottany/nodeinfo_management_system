#include <netinet/in.h>

#include "sock_wrapper_functions.h"
#include "common.h"

int update_nodeinfo(struct nodedata_list *list) {
    fprintf(stderr, "[+]: Start updating nodeinfo database\n");
    // nodeinfoデータベースを更新する処理
    // TODO: ここの処理を実装する
    // ここでは単に受信したノード情報を表示するだけにします
    for (int i = 0; i < list->current_size; i++) {
        struct nodedata *nd = &list->nodedatas[i];
        printf("Node %d: IP=%d, UserID=%d, CPU Cores=%d\n",
               i, nd->ipaddress, nd->userid, nd->cpu_core_num);
    }
    return 0;
}

int update_hostfile(struct nodedata_list *list) {
    fprintf(stderr, "[+]: Start updating /etc/hosts file\n");
    // ホストファイルを更新する処理
    // TODO: ここの処理を実装する
    // ここでは単に受信したノード情報を表示するだけにします
    FILE *fp = fopen("/etc/hosts", "a");
    if (fp == NULL) {
        perror("fopen");
        return -1;
    }
    for (int i = 0; i < list->current_size; i++) {
        struct nodedata *nd = &list->nodedatas[i];
        fprintf(fp, "%d\tuser%d\n", nd->ipaddress, nd->userid);
    }
    fclose(fp);
    return 0;
}

int receive_nodeinfo_database() {
    /*
    fprintf(stderr, "[+]: Start receiving nodeinfo database\n");

    int sock;
    struct sockaddr_in addr;
    char recv_buf[4096];
    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);
    int recv_len;

    // ソケット作成
    sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    // バインド
    addr.sin_family = AF_INET;
    addr.sin_port = htons(NODEDATA_LIST_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (wrapped_bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    // ノード情報データベース受信
    recv_len = wrapped_recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                                (struct sockaddr *)&sender_addr, &sender_addr_len);
    if (recv_len < 0) {
        close(sock);
        return -1;
    }

    // 受信データをnodeinfo_databaseにコピー
    struct nodeinfo_database *db = (struct nodeinfo_database *)recv_buf;
    if (recv_len > sizeof(*db)) {
        recv_len = sizeof(*db);
    }
    memcpy(db, recv_buf, recv_len);

    close(sock);
    return recv_len;
    */
}
