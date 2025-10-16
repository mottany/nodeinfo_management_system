#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

#include "common.h"
#include "sock_wrapper_functions.h"

// "u0_a123" から 123 を取り出す。失敗時は -1
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

struct nodedata get_my_nodedata() {
    fprintf(stderr, "[+]: Getting my nodedata\n");

    struct nodedata nd;
    nd.ipaddress = 0;
    nd.userid = -1;
    nd.cpu_core_num = 0;

    // 1) IPアドレス取得: 非ループバックのIPv4を優先
    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) == 0) {
        for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            if (ifa->ifa_addr->sa_family != AF_INET) continue;
            if (ifa->ifa_flags & IFF_LOOPBACK) continue;
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            nd.ipaddress = sa->sin_addr.s_addr; // uint32_t (network byte order)
            break;
        }
        freeifaddrs(ifaddr);
    }

    // フォールバック: UDPソケットで外向き経路の送信元IPを取得
    if (nd.ipaddress == 0) {
        int s = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
        if (s >= 0) {
            struct sockaddr_in dst;
            memset(&dst, 0, sizeof(dst));
            dst.sin_family = AF_INET;
            dst.sin_port = htons(53);
            inet_pton(AF_INET, "8.8.8.8", &dst.sin_addr);
            if (wrapped_connect(s, (struct sockaddr *)&dst, sizeof(dst)) == 0) {
                struct sockaddr_in local;
                socklen_t len = sizeof(local);
                if (getsockname(s, (struct sockaddr *)&local, &len) == 0) {
                    nd.ipaddress = local.sin_addr.s_addr;
                }
            }
            close(s);
        }
    }

    // 2) ユーザーID取得（名前 "u0_a123" -> 123）
    struct passwd *ps = getpwuid(getuid());
    int parsed = extract_numeric_userid(ps ? ps->pw_name : NULL);
    if (parsed >= 0) {
        nd.userid = parsed;
    } else {
        // 取得失敗時は -1 のまま
    }

    // 3) CPUコア数
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (cores > 0) nd.cpu_core_num = (int)cores;

    return nd;
}

int print_nodedata_list(const struct nodedata_list *list) {
    if (!list) {
        fprintf(stderr, "[-]: print_nodedata_list(): list is NULL\n");
        return -1;
    }
    printf("Nodedata List (current size: %d, max size: %d):\n",
           list->current_size, list->max_size);
    for (int i = 0; i < list->current_size; i++) {
        struct nodedata *nd = &list->nodedatas[i];
        printf("  Node %d: IP=%d, UserID=%d, CPU Cores=%d\n",
               i, nd->ipaddress, nd->userid, nd->cpu_core_num);
    }
    return 0;
}

int print_nodeinfo_database(const struct nodeinfo_database *db) {
    if (!db) {
        fprintf(stderr, "[-]: print_nodeinfo_database(): db is NULL\n");
        return -1;
    }
    printf("Nodeinfo Database (current size: %d, max size: %d):\n",
           db->current_size, db->max_size);
    for (int i = 0; i < db->current_size; i++) {
        struct nodeinfo_database_element *el = &db->elements[i];
        printf("  Element %d: NetworkID=%d, IP=%d, UserID=%d, ControlPort=%d, MessagePort=%d, CPU Cores=%d\n",
               i, el->network_id, el->ipaddress, el->userid,
               el->control_port_num, el->message_port_num, el->cpu_core_num);
    }
    return 0;
}

int update_nodeinfo(struct nodedata_list *list) {
    fprintf(stderr, "[+]: Updating %s file\n", NODEINFO);

    if (!list) {
        fprintf(stderr, "[-]: update_nodeinfo(): list is NULL\n");
        return -1;
    }

    const char *target = NODEINFO;
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s.tmp", target);

    FILE *fp = fopen(tmp, "w");
    if (!fp) {
        perror("[-]: fopen");
        return -1;
    }

    for (int i = 0; i < list->current_size; i++) {
        const struct nodedata *nd = &list->nodedatas[i];

        // IPを文字列化
        char ipstr[INET_ADDRSTRLEN];
        struct in_addr ia = { .s_addr = nd->ipaddress };
        if (!inet_ntop(AF_INET, &ia, ipstr, sizeof(ipstr))) {
            perror("[-]: inet_ntop");
            fclose(fp);
            unlink(tmp);
            return -1;
        }

        // useridが-1なら "Not Android"、それ以外は "u0_a<userid>"
        if (nd->userid < 0) {
            if (fprintf(fp, "%s %s\n", ipstr, "Not Android") < 0) {
                perror("[-]: fprintf");
                fclose(fp);
                unlink(tmp);
                return -1;
            }
        } else {
            if (fprintf(fp, "%s u0_a%d\n", ipstr, nd->userid) < 0) {
                perror("[-]: fprintf");
                fclose(fp);
                unlink(tmp);
                return -1;
            }
        }
    }

    if (fflush(fp) != 0) {
        perror("[-]: fflush");
        fclose(fp);
        unlink(tmp);
        return -1;
    }
    int fd = fileno(fp);
    if (fd >= 0 && fsync(fd) != 0) {
        perror("[-]: fsync");
        // 続行（最終的にrenameで原子更新）
    }
    if (fclose(fp) != 0) {
        perror("[-]: fclose");
        unlink(tmp);
        return -1;
    }
    if (rename(tmp, target) != 0) {
        perror("[-]: rename");
        unlink(tmp);
        return -1;
    }

    fprintf(stderr, "[+]: Updated %s with %d entries\n", target, list->current_size);
    return 0;
}

int update_hostfile(const struct nodedata_list *list) {
    fprintf(stderr, "[+]: Updating %s file\n", HOSTFILE);
    
    if (!list) {
        fprintf(stderr, "[-]: update_hostfile(): list is NULL\n");
        return -1;
    }

    const char *target = HOSTFILE;
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s.tmp", target);

    FILE *fp = fopen(tmp, "w");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    for (int i = 0; i < list->current_size; i++) {
        const struct nodedata *nd = &list->nodedatas[i];

        char ipstr[INET_ADDRSTRLEN];
        struct in_addr ia = { .s_addr = nd->ipaddress };
        if (!inet_ntop(AF_INET, &ia, ipstr, sizeof(ipstr))) {
            perror("inet_ntop");
            fclose(fp);
            unlink(tmp);
            return -1;
        }

        if (fprintf(fp, "%s %d\n", ipstr, nd->cpu_core_num) < 0) {
            perror("fprintf");
            fclose(fp);
            unlink(tmp);
            return -1;
        }
    }

    if (fflush(fp) != 0) {
        perror("fflush");
        fclose(fp);
        unlink(tmp);
        return -1;
    }
    int fd = fileno(fp);
    if (fd >= 0 && fsync(fd) != 0) {
        perror("fsync");
        // 続行（最終的にrenameで原子更新）
    }
    if (fclose(fp) != 0) {
        perror("fclose");
        unlink(tmp);
        return -1;
    }
    if (rename(tmp, target) != 0) {
        perror("rename");
        unlink(tmp);
        return -1;
    }

    fprintf(stderr, "[+]: Updated %s with %d entries\n", target, list->current_size);
    return 0;
}

int receive_nodeinfo_database() {
    
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
}
