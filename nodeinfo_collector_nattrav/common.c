#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/stat.h>

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

void print_nodedata_list(const struct nodedata_list *list) {
    if (!list) {
        fprintf(stderr, "[-]: print_nodedata_list(): list is NULL\n");
        return;
    }
    printf("Nodedata List (current size: %d, max size: %d):\n",
           list->current_size, list->max_size);
    for (int i = 0; i < list->current_size; i++) {
        struct nodedata *nd = &list->nodedatas[i];
        printf("  Node %d: IP=%d, UserID=%d, CPU Cores=%d\n",
               i, nd->ipaddress, nd->userid, nd->cpu_core_num);
    }
    return;
}

void print_nodeinfo_database(const struct nodeinfo_database *db) {
    if (!db) {
        fprintf(stderr, "[-]: print_nodeinfo_database(): db is NULL\n");
        return;
    }
    printf("Nodeinfo Database (current size: %d, max size: %d):\n",
           db->current_size, db->max_size);
    for (int i = 0; i < db->current_size; i++) {
        struct nodeinfo_database_element *el = &db->elements[i];
        printf("  Element %d: NetworkID=%d, IP=%d, UserID=%d, CPU Cores=%d, ControlPort=%d, MessagePort=%d\n",
               i, el->network_id, el->ipaddress, el->userid, el->cpu_core_num, el->control_port_num, el->message_port_num);
    }
    return;
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

    int is_new = 0;
    if (access(target, F_OK) == -1 && errno == ENOENT) {
        is_new = 1;
    }

    FILE *fp = fopen(tmp, "w");
    if (!fp) {
        perror("[-]: fopen");
        return -1;
    }

    for (int i = 0; i < list->current_size; i++) {
        const struct nodedata *nd = &list->nodedatas[i];
        char ipstr[INET_ADDRSTRLEN];
        struct in_addr ia = { .s_addr = nd->ipaddress };
        if (!inet_ntop(AF_INET, &ia, ipstr, sizeof(ipstr))) {
            perror("[-]: inet_ntop");
            fclose(fp);
            unlink(tmp);
            return -1;
        }

        if (nd->userid < 0) {
            if (fprintf(fp, "%s %s\n", ipstr, "NotAndroid") < 0) {
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
    if (is_new) {
        if (chmod(target, 0644) != 0) {
            perror("[-]: chmod");
        } else {
            fprintf(stderr, "[+]: Created %s\n", target);
        }
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

    int is_new = 0;
    if (access(target, F_OK) == -1 && errno == ENOENT) {
        is_new = 1;
    }

    FILE *fp = fopen(tmp, "w");
    if (!fp) {
        perror("[-]: fopen");
        return -1;
    }

    for (int i = 0; i < list->current_size; i++) {
        const struct nodedata *nd = &list->nodedatas[i];

        char ipstr[INET_ADDRSTRLEN];
        struct in_addr ia = { .s_addr = nd->ipaddress };
        if (!inet_ntop(AF_INET, &ia, ipstr, sizeof(ipstr))) {
            perror("[-]: inet_ntop");
            fclose(fp);
            unlink(tmp);
            return -1;
        }

        if (fprintf(fp, "%s cpu=%d\n", ipstr, nd->cpu_core_num) < 0) {
            perror("[-]: fprintf");
            fclose(fp);
            unlink(tmp);
            return -1;
        }
    }

    if (fflush(fp) != 0) {
        perror("[-]: fflush");
        fclose(fp);
        unlink(tmp);
        return -1;
    }
    int fd2 = fileno(fp);
    if (fd2 >= 0 && fsync(fd2) != 0) {
        perror("[-]: fsync");
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
    if (is_new) {
        if (chmod(target, 0644) != 0) {
            perror("[-]: chmod");
        } else {
            fprintf(stderr, "[+]: Created %s\n", target);
        }
    }

    fprintf(stderr, "[+]: Updated %s with %d entries\n", target, list->current_size);
    return 0;
}

// Internal helper to receive one DB datagram on an existing socket with timeout
static struct nodeinfo_database *receive_db_on_socket_impl(int sock,
                                                          int timeout_sec,
                                                          int timeout_usec,
                                                          int *out_bytes) {
    if (out_bytes) *out_bytes = 0;

    // apply timeout if requested
    if (timeout_sec >= 0 || timeout_usec >= 0) {
        wrapped_set_recv_timeout(sock, timeout_sec, timeout_usec);
    }

    enum { MAX_UDP_PAYLOAD = 65535 };
    char *buf = (char *)malloc(MAX_UDP_PAYLOAD);
    if (!buf) {
        if (out_bytes) *out_bytes = -1;
        return NULL;
    }

    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);
    int r = wrapped_recvfrom(sock, buf, MAX_UDP_PAYLOAD, 0,
                             (struct sockaddr *)&sender_addr, &sender_addr_len);
    if (r == IS_TIMEOUT) {
        free(buf);
        if (out_bytes) *out_bytes = IS_TIMEOUT;
        return NULL;
    }
    if (r <= 0) {
        free(buf);
        if (out_bytes) *out_bytes = -1;
        return NULL;
    }

    if (r < (int)sizeof(struct nodeinfo_database)) {
        fprintf(stderr, "[-]: DB datagram too short: %d bytes\n", r);
        free(buf);
        if (out_bytes) *out_bytes = -1;
        return NULL;
    }

    struct nodeinfo_database *hdr = (struct nodeinfo_database *)buf;
    size_t expected = sizeof(struct nodeinfo_database)
                    + sizeof(struct nodeinfo_database_element) * (size_t)hdr->current_size;
    if ((size_t)r < expected) {
        fprintf(stderr, "[-]: Corrupted DB payload (recv=%d, expected>=%zu, count=%d)\n",
                r, expected, hdr->current_size);
        free(buf);
        if (out_bytes) *out_bytes = -1;
        return NULL;
    }

    char *shrink = (char *)realloc(buf, (size_t)r);
    if (shrink) buf = shrink;

    if (out_bytes) *out_bytes = r;
    return (struct nodeinfo_database *)buf;
}

struct nodeinfo_database *receive_nodeinfo_database_on_socket(int sock,
                                                             int timeout_sec,
                                                             int timeout_usec,
                                                             int *out_bytes) {
    return receive_db_on_socket_impl(sock, timeout_sec, timeout_usec, out_bytes);
}

struct nodeinfo_database *receive_nodeinfo_database_bound(int port,
                                                         int timeout_sec,
                                                         int timeout_usec,
                                                         int *out_bytes) {
    int sock = wrapped_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        if (out_bytes) *out_bytes = -1;
        return NULL;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (wrapped_bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        if (out_bytes) *out_bytes = -1;
        return NULL;
    }

    struct nodeinfo_database *db = receive_db_on_socket_impl(sock, timeout_sec, timeout_usec, out_bytes);
    close(sock);
    return db;
}
