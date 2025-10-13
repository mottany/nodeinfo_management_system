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
#include <ctype.h>  // 追加

#include "common_asset.h"
#include "sock_wrapper_functions.h"
#include "member_node_procedure.h"

int request_join_cluster(struct sockaddr_in *master_node_addr) {
    fprintf(stderr, "[+]: Start cluster join request\n");

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

    // 受信タイムアウト設定（10秒）
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = TIMEOUT_USEC;
    setsockopt(broadcast_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // ブロードキャストアドレス設定
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(broadcast_port);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    // ブロードキャストメッセージ送信
    if (sendto(broadcast_sock, HELLO_CLUSTER_MSG, strlen(HELLO_CLUSTER_MSG), 0,
               (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        perror("[-]: sendto failed");
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
    } else if (recv_len < 0 && errno == EWOULDBLOCK) {
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

int send_my_nodedata(struct sockaddr_in *master_node_addr) {
    fprintf(stderr, "[+]: Start sending my nodedata to master node\n");
    
    int sock;
    struct nodedata my_nodedata;

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

    // 接続済みソケットから自ノードの送信元IPを取得
    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    if (getsockname(sock, (struct sockaddr *)&local_addr, &local_len) == -1) {
        perror("getsockname");
        close(sock);
        return -1;
    }

    struct passwd *ps;
    my_nodedata.ipaddress = local_addr.sin_addr.s_addr;   // uint32_t (network byte order)

    ps = getpwuid(getuid());
    int parsed_uid = extract_numeric_userid(ps ? ps->pw_name : NULL);
    if (parsed_uid < 0) {
        fprintf(stderr, "[-]: Failed to extract numeric userid from username '%s'\n",
                (ps && ps->pw_name) ? ps->pw_name : "(null)");
        close(sock);
        return -1; // フォールバックせず失敗を返す
    }
    my_nodedata.userid = parsed_uid;

    my_nodedata.cpu_core_num = (int)sysconf(_SC_NPROCESSORS_ONLN);

    fprintf(stderr, "[+]: My nodedata - IP: %s, UserID: %d, CPU Cores: %d\n",
            inet_ntoa(local_addr.sin_addr), my_nodedata.userid, my_nodedata.cpu_core_num);

    if (wrapped_send(sock, &my_nodedata, sizeof(my_nodedata), 0) < 0) {
        close(sock);
        return -1;
    }

    close(sock);
    return 0;
}

int receive_nodedata_list(struct nodedata_list *list) {
    fprintf(stderr, "[+]: Start receiving nodedata list from master node\n");

    int sock;
    struct sockaddr_in addr, sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);
    char recv_buf[1024];
    int recv_len;

    // ソケット作成
    sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    // バインド
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(NODEDATA_LIST_PORT);
    if (wrapped_bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    // ノード情報リスト受信
    recv_len = wrapped_recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                                (struct sockaddr *)&sender_addr, &sender_addr_len);
    if (recv_len < 0) {
        close(sock);
        return -1;
    }

    // 受信データをnodedata_listにコピー
    if (recv_len > sizeof(*list)) {
        recv_len = sizeof(*list);
    }
    memcpy(list, recv_buf, recv_len);

    close(sock);
    return recv_len;
}
/*
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
*/
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
    if(send_my_nodedata(&master_node_addr) == 0){
        fprintf(stderr, "[+]: Successfully sent my nodedata to master node\n");
    } else {
        fprintf(stderr, "[-]: Failed to send my nodedata to master node\n");
        return -1;
    }
    /*
    // マスターノードからのnodedata_listとnodeinfo_databaseを受信
    while (1) {
        struct nodedata_list received_list;
        int ret = receive_nodedata_list(&received_list);
        if (ret <= 0) {
            fprintf(stderr, "[-]: Failed to receive nodedata list\n");
            return -1;
        }
        update_nodeinfo(&received_list);
        update_hostfile(&received_list);
        receive_nodeinfo_database();
    }
    */
    return 0;
}
