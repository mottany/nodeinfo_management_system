#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "master_node_procedure.h"
#include "sock_wrapper_functions.h"

enum {
    TIMEOUT_CODE         = 0,
    JOIN_REQUEST_CODE    = 1,
    LEAVE_REQUEST_CODE   = 2,
    RECV_DB_REQUEST_CODE = 3
};

static const char HELLO_RELAY_SERVER_MSG[]       = "Hello_relay_server!";
static const char GIVE_ME_DB_MSG[]               = "Give_me_db";

static char RELAY_SERVER_IP[INET_ADDRSTRLEN] = "160.12.172.77";

static const int RELAY_RECV_TIMEOUT_SEC      = 1;
static const int RELAY_RECV_TIMEOUT_USEC     = 0;
static const int MEMBER_REQUEST_TIMEOUT_SEC  = 10;
static const int MEMBER_REQUEST_TIMEOUT_USEC = 0;

// Hold the latest nodeinfo_database snapshot in memory
static struct nodeinfo_database *g_nodeinfo_db = NULL;
static size_t g_nodeinfo_db_bytes = 0;

static void init_relay_server_ip(void) {
    const char *env = getenv("RELAY_SERVER_IP");
    if (env && *env) {
        struct in_addr tmp;
        if (inet_pton(AF_INET, env, &tmp) == 1) {
            // valid IPv4 string; copy safely
            strncpy(RELAY_SERVER_IP, env, sizeof(RELAY_SERVER_IP) - 1);
            RELAY_SERVER_IP[sizeof(RELAY_SERVER_IP) - 1] = '\0';
            fprintf(stderr, "[+]: Using RELAY_SERVER_IP from env: %s\n", RELAY_SERVER_IP);
        } else {
            fprintf(stderr, "[-]: Ignoring invalid RELAY_SERVER_IP env: %s\n", env);
        }
    }
}

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

static int request_join_huge_cluster() {
    // 中継サーバにクラスタ参加要求: HELLO を送り、network_id(uint32_t)を受け取る
    fprintf(stderr, "[+]: Requesting to join huge cluster via relay server\n");

    int sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    // 受信タイムアウト設定
    // 受信タイムアウト設定（ラッパー経由）
    wrapped_set_recv_timeout(sock, RELAY_RECV_TIMEOUT_SEC, RELAY_RECV_TIMEOUT_USEC);

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
    if (r == IS_TIMEOUT) {
        close(sock);
        return IS_TIMEOUT;
    } else if (r <= 0) {
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

static int accept_member_request() {
    fprintf(stderr, "[+]: Waiting for control message from member nodes\n");

    int sock;
    struct sockaddr_in addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buf[256];

    /* UDP ソケット作成 */
    sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    /* 受信タイムアウトを設定（一定時間リクエストがなければ TIMEOUT_CODE を返す） */
    wrapped_set_recv_timeout(sock, MEMBER_REQUEST_TIMEOUT_SEC, MEMBER_REQUEST_TIMEOUT_USEC);

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
    if (r == IS_TIMEOUT) {
        /* 規定時間内にメンバーからの要求がなければ TIMEOUT_CODE */
        close(sock);
        return TIMEOUT_CODE;
    } else if (r <= 0) {
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
    } else {
        fprintf(stderr, "[-]: Unknown control message: %s\n", buf);
        close(sock);
        return -1;
    }
}

static struct nodedata receive_nodedata() {
    fprintf(stderr, "[+]: Receiving nodedata from member node\n");

    struct nodedata result;
    memset(&result, 0, sizeof(result));
    result.userid = -1; // 明示的に無効値をセット

    int listen_sock, conn_sock;
    struct sockaddr_in addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int opt = 1;

    listen_sock = wrapped_socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        return result;
    }

    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(NODEDATA_PORT);

    if (wrapped_bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(listen_sock);
        return result;
    }

    if (wrapped_listen(listen_sock, 1) < 0) {
        close(listen_sock);
        return result;
    }

    conn_sock = wrapped_accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
    if (conn_sock < 0) {
        close(listen_sock);
        return result;
    }

    // 構造体サイズぶんを受信（短い受信に備えてループ）
    size_t need = sizeof(result);
    size_t got = 0;
    while (got < need) {
        int r = wrapped_recv(conn_sock, ((char *)&result) + got, need - got, 0);
        if (r <= 0) {
            close(conn_sock);
            close(listen_sock);
            memset(&result, 0, sizeof(result));
            result.userid = -1;
            return result;
        }
        got += (size_t)r;
    }

    close(conn_sock);
    close(listen_sock);
    return result;
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

static int send_nodedata_list_to_members(const struct nodedata_list *list) {
    if (!list) {
        return -1;
    }

    size_t payload_len = sizeof(struct nodedata_list) +
                         sizeof(struct nodedata) * (size_t)list->current_size;

    int sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    int failures = 0;
    for (int i = 0; i < list->current_size; i++) {
        uint32_t ip = list->nodedatas[i].ipaddress;
        if (ip == 0) {
            continue;
        }
        // skip self
        struct nodedata me = get_my_nodedata();
        if (ip == me.ipaddress) continue;

        struct sockaddr_in dst;
        memset(&dst, 0, sizeof(dst));
        dst.sin_family = AF_INET;
        // 1) Send control message to CTRL_MSG_PORT
        dst.sin_port = htons(CTRL_MSG_PORT);
        dst.sin_addr.s_addr = ip;

        ssize_t n = sendto(sock, READY_SEND_NODEDATA_LIST_MSG, strlen(READY_SEND_NODEDATA_LIST_MSG), 0,
                           (struct sockaddr *)&dst, sizeof(dst));
        if (n < 0 || (size_t)n != strlen(READY_SEND_NODEDATA_LIST_MSG)) {
            perror("[-]: sendto(control READY_SEND_NODEDATA_LIST)");
            failures++;
            continue;
        }

        // 2) Send payload to NODEDATA_LIST_PORT
        dst.sin_port = htons(NODEDATA_LIST_PORT);
        n = sendto(sock, list, payload_len, 0,
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

static int relay_request_ready_for_nodedata_list(void) {
    int sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    // 受信タイムアウト（リレー応答待ちに適用）
    wrapped_set_recv_timeout(sock, RELAY_RECV_TIMEOUT_SEC, RELAY_RECV_TIMEOUT_USEC);

    struct sockaddr_in relay;
    memset(&relay, 0, sizeof(relay));
    relay.sin_family = AF_INET;
    relay.sin_port = htons(CTRL_MSG_PORT);
    if (inet_pton(AF_INET, RELAY_SERVER_IP, &relay.sin_addr) != 1) {
        fprintf(stderr, "[-]: inet_pton failed for %s\n", RELAY_SERVER_IP);
        close(sock);
        return -1;
    }

    size_t ctrl_len = strlen(READY_SEND_NODEDATA_LIST_MSG);
    if (sendto(sock, READY_SEND_NODEDATA_LIST_MSG, ctrl_len, 0,
               (struct sockaddr *)&relay, sizeof(relay)) < 0) {
        perror("[-]: sendto(READY_SEND_NODEDATA_LIST_MSG)");
        close(sock);
        return -1;
    }

    char ack[128];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    int r = wrapped_recvfrom(sock, ack, sizeof(ack) - 1, 0,
                             (struct sockaddr *)&from, &fromlen);
    if (r == IS_TIMEOUT) {
        close(sock);
        return IS_TIMEOUT;
    } else if (r <= 0) {
        close(sock);
        return -1;
    }
    ack[r] = '\0';
    if (strcmp(ack, READY_RECV_NODEDATA_LIST_MSG) != 0) {
        fprintf(stderr, "[-]: Unexpected ack from relay: '%s'\n", ack);
        close(sock);
        return -1;
    }

    close(sock);
    return 0;
}

static int send_nodedata_list_to_relay_server(const struct nodedata_list *list) {
    fprintf(stderr, "[+]: Sending nodedata_list to relay server\n");
    if (!list) {
        fprintf(stderr, "[-]: send_nodedata_list_to_relay_server(): list is NULL\n");
        return -1;
    }

    // 送信サイズはヘッダ + 現在要素数ぶんの可変配列
    size_t payload_len = sizeof(struct nodedata_list) +
                         sizeof(struct nodedata) * (size_t)list->current_size;

    int sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    struct sockaddr_in relay;
    memset(&relay, 0, sizeof(relay));
    relay.sin_family = AF_INET;
    relay.sin_port = htons(NODEDATA_LIST_PORT);
    if (inet_pton(AF_INET, RELAY_SERVER_IP, &relay.sin_addr) != 1) {
        fprintf(stderr, "[-]: inet_pton failed for RELAY_SERVER_IP=%s\n", RELAY_SERVER_IP);
        close(sock);
        return -1;
    }

    ssize_t n = sendto(sock, list, payload_len, 0,
                       (struct sockaddr *)&relay, sizeof(relay));
    if (n < 0 || (size_t)n != payload_len) {
        perror("[-]: sendto(nodedata_list to relay server)");
        close(sock);
        return -1;
    }

    fprintf(stderr, "[+]: Sent nodedata_list (count=%d) to relay server %s:%d\n",
            list->current_size, RELAY_SERVER_IP, NODEDATA_LIST_PORT);

    close(sock);
    return 0;
}

static int distribute_nodedata_list(const struct nodedata_list *list) {
    fprintf(stderr, "[+]: Distributing nodedata_list to member nodes and relay server\n");
    if (!list || list->current_size < 0 || list->max_size <= 0) {
        fprintf(stderr, "[-]: distribute_nodedata_list(): invalid list\n");
        return -1;
    }

    if (send_nodedata_list_to_members(list) < 0) {
        fprintf(stderr, "[-]: Some member deliveries failed\n");
        return -1;
    }
    // リレー参加がタイムアウトしている場合は、リレー関連の送信をスキップ
    if (list->network_id == IS_TIMEOUT) {
        fprintf(stderr, "[!]: Skipping relay distribution due to previous join timeout\n");
        return 0;
    }

    {
        int r = relay_request_ready_for_nodedata_list();
        if (r == IS_TIMEOUT) {
            fprintf(stderr, "[!]: Relay ready timeout; skip sending to relay this round\n");
            return 0;
        }
        if (r < 0) {
            return -1;
        }
    }
    if (send_nodedata_list_to_relay_server(list) < 0) {
        return -1;
    }
    return 0;
}

// Request nodeinfo_database from relay server and keep it in memory
static int request_nodeinfo_database(void) {
    fprintf(stderr, "[+]: Requesting nodeinfo_database from relay server\n");

    int sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }
    // apply receive timeout for control/data from relay
    wrapped_set_recv_timeout(sock, RELAY_RECV_TIMEOUT_SEC, RELAY_RECV_TIMEOUT_USEC);

    struct sockaddr_in relay;
    memset(&relay, 0, sizeof(relay));
    relay.sin_family = AF_INET;
    // Use the control port so the relay can reply on the same socket (NAT friendly)
    relay.sin_port = htons(CTRL_MSG_PORT);
    if (inet_pton(AF_INET, RELAY_SERVER_IP, &relay.sin_addr) != 1) {
        fprintf(stderr, "[-]: inet_pton failed for RELAY_SERVER_IP=%s\n", RELAY_SERVER_IP);
        close(sock);
        return -1;
    }

    // 1) Send GIVE_ME_DB_MSG
    size_t mlen = strlen(GIVE_ME_DB_MSG);
    if (sendto(sock, GIVE_ME_DB_MSG, mlen, 0, (struct sockaddr *)&relay, sizeof(relay)) < 0) {
        perror("[-]: sendto(GIVE_ME_DB_MSG)");
        close(sock);
        return -1;
    }

    // 2) Receive the DB datagram directly
    struct nodeinfo_database *db = receive_nodeinfo_database_on_socket(sock);
    if (!db) {
        close(sock);
        return -1;
    }

    if (g_nodeinfo_db) free(g_nodeinfo_db);
    g_nodeinfo_db = db;

    fprintf(stderr, "[+]: Updated in-memory nodeinfo_database (elements=%d, bytes=%zu)\n",
            g_nodeinfo_db->current_size, g_nodeinfo_db_bytes);
    print_nodeinfo_database(g_nodeinfo_db);

    close(sock);
    return 0;
}

static int distribute_nodeinfo_database(const struct nodedata_list *list) {
    fprintf(stderr, "[+]: Distributing nodeinfo_database to member nodes\n");
    if (!list) {
        fprintf(stderr, "[-]: distribute_nodeinfo_database(): list is NULL\n");
        return -1;
    }
    if (!g_nodeinfo_db) {
        fprintf(stderr, "[!]: distribute_nodeinfo_database(): no nodeinfo_database to send\n");
        return 0; // nothing to send is not a hard error
    }

    // 自ノードIP（送信対象から除外するため）
    struct nodedata me = get_my_nodedata();
    uint32_t my_ip = me.ipaddress;

    size_t payload_len = sizeof(struct nodeinfo_database) +
                         sizeof(struct nodeinfo_database_element) * (size_t)g_nodeinfo_db->current_size;

    int sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    int failures = 0;
    for (int i = 0; i < list->current_size; i++) {
        uint32_t ip = list->nodedatas[i].ipaddress;
        if (ip == 0) continue;
        if (ip == my_ip) continue; // skip self

        struct sockaddr_in dst;
        memset(&dst, 0, sizeof(dst));
        dst.sin_family = AF_INET;
        // 1) Send control message for DB
        dst.sin_port = htons(CTRL_MSG_PORT);
        dst.sin_addr.s_addr = ip;


        ssize_t n = sendto(sock, READY_SEND_DB_MSG, strlen(READY_SEND_DB_MSG), 0,
                           (struct sockaddr *)&dst, sizeof(dst));
        if (n < 0 || (size_t)n != strlen(READY_SEND_DB_MSG)) {
            perror("[-]: sendto(control READY_SEND_DB)");
            failures++;
            continue;
        }

        // 2) Send DB payload to DB port
        dst.sin_port = htons(NODEINFO_DB_PORT);
        n = sendto(sock, g_nodeinfo_db, payload_len, 0,
                   (struct sockaddr *)&dst, sizeof(dst));
        if (n < 0 || (size_t)n != payload_len) {
            perror("[-]: sendto(nodeinfo_database to member)");
            failures++;
            continue;
        }

        char ipstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &dst.sin_addr, ipstr, sizeof(ipstr));
        fprintf(stderr, "[+]: Sent nodeinfo_database (count=%d) to %s:%d\n",
                g_nodeinfo_db->current_size, ipstr, NODEINFO_DB_PORT);
    }

    close(sock);
    return failures ? -1 : 0;
}

int run_master_node_procedure() {
    fprintf(stderr, "[+]: Start master node procedure\n");

    // allow overriding RELAY_SERVER_IP via environment
    init_relay_server_ip();

    // nodedata_listの作成（自ノード情報を格納）
    struct nodedata_list *nd_list = create_nodedata_list();
    if (!nd_list) {
        fprintf(stderr, "[-]: Failed to create nodedata_list\n");
        return -1;
    }

    int network_id = request_join_huge_cluster();
    if (network_id == IS_TIMEOUT) {
        fprintf(stderr, "[!]: Relay join timed out; will skip relay distribution.\n");
        nd_list->network_id = IS_TIMEOUT;
    } else if (network_id < 0) {
        fprintf(stderr, "[-]: Failed to join huge cluster via relay server\n");
        return -1;
    } else {
        nd_list->network_id = network_id;
    }

    while(1){
        int request_code = accept_member_request();        
        if(request_code < 0){
            fprintf(stderr, "[-]: Error in accepting request\n");
            return -1;
        }

        // メンバノードから一定時間リクエストがなかったら
        if(request_code == TIMEOUT_CODE){
            if(nd_list->network_id > 0){ 
                if(request_nodeinfo_database() < 0){
                   fprintf(stderr, "[-]: Failed to request nodeinfo_database\n");
                   return -1;
                }
                // 受領したDBをメンバーへ配布（タイムアウト分岐内）
                if (distribute_nodeinfo_database(nd_list) < 0) {
                    fprintf(stderr, "[-]: Some nodeinfo_database deliveries failed\n");
                    return -1;
                }
            }
            continue;
        }
        
        struct nodedata nd = receive_nodedata();
        if (nd.ipaddress == 0 || /*nd.userid < 0 ||*/ nd.cpu_core_num <= 0) {
            fprintf(stderr, "[-]: Failed to receive valid nodedata from member node\n");
            return -1;
        }
        fprintf(stderr, "[+]: Successfully received nodedata from %s (uid=%d, cpu=%d)\n",
            inet_ntoa(*(struct in_addr *)&nd.ipaddress), nd.userid, nd.cpu_core_num);
        
        // メンバノードからクラスタ参加要求を受信したら
        if(request_code == JOIN_REQUEST_CODE){
            if(add_nodedata_to_list(&nd, nd_list) < 0){
                fprintf(stderr, "[-]: Failed to add nodedata to list\n");
                return -1;
            }
            fprintf(stderr, "[+]: Successfully added nodedata to list (current size: %d)\n", nd_list->current_size);
        }
        
        // メンバノードからクラスタ脱退要求を受信したら
        // else if (request_code == LEAVE_REQUEST_CODE){}
        
        print_nodedata_list(nd_list);
        
        if(distribute_nodedata_list(nd_list) < 0){
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
        if(request_nodeinfo_database() < 0){
            fprintf(stderr, "[-]: Failed to request nodeinfo_database\n");
            return -1;
        }
        // 受領したDBをメンバーへ配布
        if (distribute_nodeinfo_database(nd_list) < 0) {
            fprintf(stderr, "[!]: Some nodeinfo_database deliveries failed\n");
        }
    }
    
    return 0;
};