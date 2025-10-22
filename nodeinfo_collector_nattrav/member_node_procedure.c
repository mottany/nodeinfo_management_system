#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <stdlib.h>
#include <pwd.h>
#include <ctype.h>

#include "common.h"
#include "sock_wrapper_functions.h"
#include "member_node_procedure.h"

static const int RECV_TIMEOUT_SEC = 1;
static const int RECV_TIMEOUT_USEC = 0;

static int request_join_cluster(struct sockaddr_in *master_node_addr) {
    fprintf(stderr, "[+]: Requesting cluster join\n");

    int broadcast_sock;
    struct sockaddr_in broadcast_addr;
    const int broadcast_port = CTRL_MSG_PORT;
    char recv_buf[256];

    // ブロードキャスト用ソケット作成
    broadcast_sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcast_sock < 0) {
        return -1; 
    }

    // ブロードキャスト許可
    int broadcast_enable = 1;
    setsockopt(broadcast_sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));

    // 受信タイムアウト設定（ラッパー経由）
    wrapped_set_recv_timeout(broadcast_sock, RECV_TIMEOUT_SEC, RECV_TIMEOUT_USEC);

    // ブロードキャストアドレス設定
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(broadcast_port);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    // ブロードキャストメッセージ送信
    if (wrapped_sendto(broadcast_sock, (void*)HELLO_CLUSTER_MSG, strlen(HELLO_CLUSTER_MSG), 0,
               (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        close(broadcast_sock);
        return -1;
    }

    // マスターノードからの応答受信
    struct sockaddr_in master_addr;
    socklen_t addrlen = sizeof(master_addr);
    int recv_len = wrapped_recvfrom(broadcast_sock, recv_buf, sizeof(recv_buf) - 1, 0,
                                    (struct sockaddr *)&master_addr, &addrlen);
    if (recv_len > 0) {
        recv_buf[recv_len] = '\0';
        *master_node_addr = master_addr;
        close(broadcast_sock);
        return recv_len;
    } else if (recv_len == IS_TIMEOUT) {
        fprintf(stderr, "[-]: Timeout waiting for master node response\n");
        close(broadcast_sock);
        return IS_TIMEOUT;
    } else {
        fprintf(stderr, "[-]: Error receiving response\n");
        close(broadcast_sock);
        return -1;
    }
}

static int extract_numeric_userid(const char *name) {
    if (!name) return -1;

    const char *prefix = "u0_a";
    const char *p = name;

    if (strncmp(name, prefix, strlen(prefix)) == 0) {
        p = name + strlen(prefix);
    } else {
        while (*p && !isdigit((unsigned char)*p)) p++;
    }

    long v = 0;
    int have = 0;
    while (*p && isdigit((unsigned char)*p)) {
        v = v * 10 + (*p - '0');
        have = 1;
        p++;
    }
    if (!have) return -1;
    return (int)v;
}

static int send_my_nodedata(struct sockaddr_in *master_node_addr) {
    fprintf(stderr, "[+]: Sending my nodedata to master node\n");
    
    int sock;
    struct nodedata my_nodedata = {0};

    sock = wrapped_socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }

    // 接続先は master の IP + NODEDATA_PORT にする
    struct sockaddr_in dest = *master_node_addr;
    dest.sin_port = htons(NODEDATA_PORT);

    if (wrapped_connect(sock, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        close(sock);
        return -1;
    }

    // 共通関数から自ノード情報取得
    my_nodedata = get_my_nodedata();
    if (my_nodedata.ipaddress == 0 || /*my_nodedata.userid < 0 ||*/ my_nodedata.cpu_core_num <= 0) {
        fprintf(stderr, "[-]: get_my_nodedata() returned invalid data (ip=%u uid=%d cpu=%d)\n",
                my_nodedata.ipaddress, my_nodedata.userid, my_nodedata.cpu_core_num);
        close(sock);
        return -1;
    }

    // デバッグ表示
    {
        char ipstr[INET_ADDRSTRLEN];
        struct in_addr ia = { .s_addr = my_nodedata.ipaddress };
        inet_ntop(AF_INET, &ia, ipstr, sizeof(ipstr));
        fprintf(stderr, "[+]: My nodedata - IP: %s, UserID: %d, CPU Cores: %d\n",
                ipstr, my_nodedata.userid, my_nodedata.cpu_core_num);
    }

    if (wrapped_send(sock, &my_nodedata, sizeof(my_nodedata), 0) < 0) {
        close(sock);
        return -1;
    }

    close(sock);
    return 0;
}

static struct nodedata_list *receive_nodedata_list(void) {
    fprintf(stderr, "[+]: Receiving nodedata list from master node\n");

    int sock;
    struct sockaddr_in addr, sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);
    // 可変長に対応するため十分大きい受信バッファを確保（最大UDPペイロード近辺）
    enum { MAX_UDP_PAYLOAD = 65535 };
    char *recv_buf = (char *)malloc(MAX_UDP_PAYLOAD);
    if (!recv_buf) return NULL;

    // ソケット作成
    sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        free(recv_buf);
        return NULL;
    }

    // バインド
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(NODEDATA_LIST_PORT);
    if (wrapped_bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        free(recv_buf);
        return NULL;
    }

    // ノード情報リスト受信（1Datagramで全体が届く想定）
    int recv_len = wrapped_recvfrom(sock, recv_buf, MAX_UDP_PAYLOAD, 0,
                                    (struct sockaddr *)&sender_addr, &sender_addr_len);
    if (recv_len <= 0) {
        close(sock);
        free(recv_buf);
        return NULL;
    }
    if (recv_len < (int)sizeof(struct nodedata_list)) {
        fprintf(stderr, "[-]: Received too short nodedata_list (%d bytes)\n", recv_len);
        close(sock);
        free(recv_buf);
        return NULL;
    }

    struct nodedata_list *list = (struct nodedata_list *)malloc((size_t)recv_len);
    if (!list) {
        close(sock);
        free(recv_buf);
        return NULL;
    }
    memcpy(list, recv_buf, (size_t)recv_len);

    // サイズ整合性の簡易検証
    size_t expected = sizeof(struct nodedata_list) +
                      sizeof(struct nodedata) * (size_t)list->current_size;
    if ((size_t)recv_len < expected) {
        fprintf(stderr, "[-]: Corrupted nodedata_list (recv=%d, expected>=%zu, count=%d)\n",
                recv_len, expected, list->current_size);
        close(sock);
        free(recv_buf);
        free(list);
        return NULL;
    }

    close(sock);
    free(recv_buf);
    return list;
}

int run_member_node_procedure(){
    fprintf(stderr, "[+]: Start member node procedure\n");
    
    struct sockaddr_in master_node_addr;

    // ブロードキャストでクラスタ参加要求＆マスターノード情報取得
    int recv_len = request_join_cluster(&master_node_addr);
    if (recv_len == IS_TIMEOUT) {
        return IS_TIMEOUT;
    } else if (recv_len == -1) {
        return -1;
    } else if (recv_len > 0) {
        fprintf(stderr, "[+]: Received master node info: %s:%d\n",
                inet_ntoa(master_node_addr.sin_addr), ntohs(master_node_addr.sin_port));
        fprintf(stderr, "[+]: Successfully joined the cluster\n");
    }
    
    // マスターノードにmy_nodedataを送信
    if(send_my_nodedata(&master_node_addr) < 0){
        fprintf(stderr, "[-]: Failed to send my nodedata to master node\n");
        return -1;
    }
    fprintf(stderr, "[+]: Successfully sent my nodedata to master node\n");
    
    // マスターノードからのnodedata_listとnodeinfo_databaseを受信
    while (1) {
        struct nodedata_list *received_list = receive_nodedata_list();
        if (!received_list) {
            fprintf(stderr, "[-]: Failed to receive nodedata list\n");
            return -1;
        }
        fprintf(stderr, "[+]: Successfully received nodedata list (count=%d)\n",
                received_list->current_size);
        print_nodedata_list(received_list);
        if (update_nodeinfo(received_list) < 0) {
            fprintf(stderr, "[-]: Failed to update nodeinfo file\n");
            free(received_list);
            return -1;
        }
        if (update_hostfile(received_list) < 0) {
            fprintf(stderr, "[-]: Failed to update hostfile\n");
            free(received_list);
            return -1;
        }
        fprintf(stderr, "[+]: Successfully updated nodeinfo and hostfile\n");
        free(received_list);
        // receive_nodeinfo_database();
     }
    
    return 0;
}
