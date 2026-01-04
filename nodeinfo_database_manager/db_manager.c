#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "db_types.h"
#include "db_manager.h"

static const char *format_ipv4_from_u32(uint32_t ipv4_net_order, char *buf, size_t buflen) {
    if (!buf || buflen == 0) return "";
    struct in_addr ia;
    ia.s_addr = ipv4_net_order;
    if (!inet_ntop(AF_INET, &ia, buf, buflen)) {
        snprintf(buf, buflen, "<invalid ip>");
    }
    return buf;
}

static const char *format_username_from_userid(int userid, char *buf, size_t buflen) {
    if (!buf || buflen == 0) return "";
    if (userid < 0) {
        snprintf(buf, buflen, "NotAndroid");
    } else {
        snprintf(buf, buflen, "u0_a%d", userid);
    }
    return buf;
}

// Internal DB state
static struct nodeinfo_database *g_db = NULL;
static int g_db_capacity = 0;
static const int PORT_BASE = 10000;
static int next_port = 10000;

static int ensure_db_capacity(int need) {
    if (g_db && g_db_capacity >= need) return 0;
    int new_cap = g_db_capacity > 0 ? g_db_capacity : 16;
    while (new_cap < need) new_cap *= 2;
    size_t bytes = sizeof(struct nodeinfo_database) + sizeof(struct nodeinfo_database_element) * (size_t)new_cap;
    struct nodeinfo_database *newdb = (struct nodeinfo_database *)malloc(bytes);
    if (!newdb) return -1;
    newdb->max_size = new_cap;
    newdb->current_size = 0;
    if (g_db) {
        newdb->current_size = g_db->current_size;
        memcpy(newdb->elements, g_db->elements, sizeof(struct nodeinfo_database_element) * (size_t)g_db->current_size);
        free(g_db);
    }
    g_db = newdb;
    g_db_capacity = new_cap;
    return 0;
}

static int is_port_in_db(int port) {
    if (!g_db) return 0;
    for (int i = 0; i < g_db->current_size; i++) {
        if (g_db->elements[i].control_port_num == port || g_db->elements[i].message_port_num == port) {
            return 1;
        }
    }
    return 0;
}

static int is_port_available_os(int port) {
    int sock;
    struct sockaddr_in addr;

    // TCP check
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return 0;
    }
    close(sock);

    // UDP check
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return 0;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return 0;
    }
    close(sock);
    return 1;
}

static int find_next_free_port(void) {
    if (next_port < PORT_BASE) next_port = PORT_BASE;
    for (int p = next_port; p <= 65535; p++) {
        if (is_port_in_db(p)) continue;
        if (is_port_available_os(p)) {
            next_port = p + 1;
            return p;
        }
    }
    for (int p = PORT_BASE; p < next_port && p <= 65535; p++) {
        if (is_port_in_db(p)) continue;
        if (is_port_available_os(p)) {
            next_port = p + 1;
            return p;
        }
    }
    return -1;
}

static int exists_in_db(int network_id, uint32_t ipaddr) {
    if (!g_db) return 0;
    for (int i = 0; i < g_db->current_size; i++) {
        if (g_db->elements[i].network_id == network_id && g_db->elements[i].ipaddress == ipaddr) {
            return 1;
        }
    }
    return 0;
}

static int ip_present_in_list(const struct nodedata_list *list, uint32_t ipaddr) {
    if (!list) return 0;
    for (int i = 0; i < list->current_size; i++) {
        uint32_t lip = (uint32_t)list->nodedatas[i].ipaddress;
        if (lip == ipaddr) return 1;
    }
    return 0;
}

struct nodeinfo_database *update_nodeinfo_database(const struct nodedata_list *list) {
    if (!list) {
        fprintf(stderr, "[-]: update_nodeinfo_database: list is NULL\n");
        return NULL;
    }

    int target_nid = list->network_id;
    fprintf(stderr, "[+]: Updating nodeinfo_database for network_id=%d\n", target_nid);

    // 1) 削除: list に存在しないノードを DB から除去（対象 network_id のみ）
    if (g_db) {
        int removed = 0;
        for (int i = 0; i < g_db->current_size; ) {
            struct nodeinfo_database_element *e = &g_db->elements[i];
            if (e->network_id == target_nid && !ip_present_in_list(list, e->ipaddress)) {
                struct in_addr ia = { .s_addr = e->ipaddress };
                char ipstr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &ia, ipstr, sizeof(ipstr));
                fprintf(stderr, "[!]: Removing node from DB: nid=%d ip=%s (not in list)\n", target_nid, ipstr);
                // 削除: 後ろから詰める（順序は保持しない）
                g_db->elements[i] = g_db->elements[g_db->current_size - 1];
                g_db->current_size--;
                removed++;
                continue; // i は据え置き
            }
            i++;
        }
        if (removed) {
            fprintf(stderr, "[+]: Removed %d obsolete entries (after removal size=%d)\n", removed, g_db->current_size);
        }
    }

    // 2) 追加のための容量確保（上限見積もりで確保しておく）
    int need_capacity = (g_db ? g_db->current_size : 0) + list->current_size;
    if (ensure_db_capacity(need_capacity < 16 ? 16 : need_capacity) < 0) {
        fprintf(stderr, "[-]: Failed to ensure db capacity\n");
        return NULL;
    }

    int added = 0;
    for (int i = 0; i < list->current_size; i++) {
        const struct nodedata *nd = &list->nodedatas[i];
        uint32_t ip = (uint32_t)nd->ipaddress;
        if (exists_in_db(target_nid, ip)) continue;

        int ctrl_port = find_next_free_port();
        if (ctrl_port < 0) {
            fprintf(stderr, "[-]: No free control port available\n");
            continue;
        }
        int msg_port = find_next_free_port();
        if (msg_port < 0) {
            fprintf(stderr, "[-]: No free message port available\n");
            continue;
        }

        if (g_db->current_size >= g_db->max_size) {
            if (ensure_db_capacity(g_db->current_size + 1) < 0) {
                fprintf(stderr, "[-]: Failed to grow db\n");
                break;
            }
        }

        struct nodeinfo_database_element *e = &g_db->elements[g_db->current_size++];
        e->network_id = target_nid;
        e->ipaddress = ip;
        e->userid = nd->userid;
        e->cpu_core_num = nd->cpu_core_num;
        e->control_port_num = ctrl_port;
        e->message_port_num = msg_port;

        struct in_addr ia = { .s_addr = ip };
        char ipstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ia, ipstr, sizeof(ipstr));
        fprintf(stderr, "[+]: Added node to DB: nid=%d ip=%s uid=%d cpu=%d ctrl=%d msg=%d\n",
                target_nid, ipstr, e->userid, e->cpu_core_num, ctrl_port, msg_port);
        added++;
    }

    fprintf(stderr, "[+]: DB update done. current_size=%d (added=%d)\n", g_db->current_size, added);

    size_t out_bytes = sizeof(struct nodeinfo_database) + sizeof(struct nodeinfo_database_element) * (size_t)g_db->current_size;
    struct nodeinfo_database *out = (struct nodeinfo_database *)malloc(out_bytes);
    if (!out) {
        fprintf(stderr, "[-]: malloc failed for output db\n");
        return NULL;
    }
    out->max_size = g_db->current_size;
    out->current_size = g_db->current_size;
    memcpy(out->elements, g_db->elements, sizeof(struct nodeinfo_database_element) * (size_t)g_db->current_size);
    return out;
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
        char ipstr[INET_ADDRSTRLEN];
        char uname[32];
        printf("  Element %d: NetworkID=%d, IP=%s, User=%s, CPU Cores=%d, ControlPort=%d, MessagePort=%d\n",
               i,
               el->network_id,
               format_ipv4_from_u32(el->ipaddress, ipstr, sizeof(ipstr)),
               format_username_from_userid(el->userid, uname, sizeof(uname)),
               el->cpu_core_num,
               el->control_port_num,
               el->message_port_num);
    }
    return;
}
