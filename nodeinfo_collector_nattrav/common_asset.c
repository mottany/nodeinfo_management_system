#include <netinet/in.h>

#include "sock_wrapper_functions.h"
#include "common_asset.h"

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
