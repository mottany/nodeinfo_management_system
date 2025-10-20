#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>

#include "db_types.h"
#include "db_manager.h"

static struct nodedata_list *make_list(int network_id, const char *ips[], const int uids[], const int cpus[], int count) {
    size_t bytes = sizeof(struct nodedata_list) + sizeof(struct nodedata) * (size_t)count;
    struct nodedata_list *list = (struct nodedata_list *)malloc(bytes);
    if (!list) return NULL;
    list->max_size = count;
    list->current_size = count;
    list->network_id = network_id;
    for (int i = 0; i < count; i++) {
        struct in_addr ia; inet_pton(AF_INET, ips[i], &ia);
        list->nodedatas[i].ipaddress = (int)ia.s_addr; // network byte order
        list->nodedatas[i].userid = uids ? uids[i] : (1000 + i);
        list->nodedatas[i].cpu_core_num = cpus ? cpus[i] : (4 + i);
    }
    return list;
}

static void assert_elem_ports_valid(const struct nodeinfo_database_element *e) {
    assert(e->control_port_num >= 10000);
    assert(e->message_port_num >= 10000);
    assert(e->control_port_num != e->message_port_num);
}

int main(void) {
    fprintf(stderr, "[TEST]: update_nodeinfo_database begin\n");

    // Case 1: add two nodes for nid=1
    const char *ips1[] = {"10.0.0.1", "10.0.0.2"};
    struct nodedata_list *l1 = make_list(1, ips1, NULL, NULL, 2);
    assert(l1);
    struct nodeinfo_database *db1 = update_nodeinfo_database(l1);
    assert(db1);
    assert(db1->current_size == 2);
    for (int i = 0; i < db1->current_size; i++) {
        assert(db1->elements[i].network_id == 1);
        assert_elem_ports_valid(&db1->elements[i]);
    }
    free(l1);
    free(db1);

    // Case 2: delete one node (keep only 10.0.0.2)
    const char *ips2[] = {"10.0.0.2"};
    struct nodedata_list *l2 = make_list(1, ips2, NULL, NULL, 1);
    assert(l2);
    struct nodeinfo_database *db2 = update_nodeinfo_database(l2);
    assert(db2);
    assert(db2->current_size == 1);
    free(l2);
    free(db2);

    // Case 3: re-add 10.0.0.1 (now list has 10.0.0.2 and 10.0.0.1)
    const char *ips3[] = {"10.0.0.2", "10.0.0.1"};
    struct nodedata_list *l3 = make_list(1, ips3, NULL, NULL, 2);
    struct nodeinfo_database *db3 = update_nodeinfo_database(l3);
    assert(db3);
    assert(db3->current_size == 2);
    free(l3);
    free(db3);

    // Case 4: idempotent update (same list again)
    const char *ips4[] = {"10.0.0.2", "10.0.0.1"};
    struct nodedata_list *l4 = make_list(1, ips4, NULL, NULL, 2);
    struct nodeinfo_database *db4 = update_nodeinfo_database(l4);
    assert(db4);
    assert(db4->current_size == 2);
    free(l4);
    free(db4);

    fprintf(stderr, "[TEST]: update_nodeinfo_database OK\n");
    return 0;
}
