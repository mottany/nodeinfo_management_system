#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define HELLO_CLUSTER_MSG "Hello_cluster!"
#define WELCOME_NODE_MSG "Welcome_node!"
#define BYE_CLUSTER_MSG "Bye_cluster"
#define BYE_NODE_MSG "Bye_node"
#define READY_SEND_DB_MSG "Ready_to_send_DB"
#define READY_RECV_DB_MSG "Ready_to_recv_DB"
#define NODEINFO "nodeinfo"
#define HOSTFILE "hostfile"

#define CTRL_MSG_PORT 8000
#define NODEDATA_PORT 8001
#define NODEDATA_LIST_PORT 8002
#define NODEINFO_DB_PORT 8003

#define INITIAL_SIZE_OF_NODEDATA_LIST 16

#define IS_TIMEOUT (-2)

struct nodedata {
    uint32_t ipaddress;
    int userid;
    int cpu_core_num;
};

struct nodedata_list {
    int max_size;
    int current_size;
    int network_id;
    struct nodedata nodedatas[];
};

struct nodeinfo_database_element {
    int network_id;
    uint32_t ipaddress;
    int userid;
    int control_port_num;
    int message_port_num;
    int cpu_core_num;
};

struct nodeinfo_database {
    int max_size;
    int current_size;
    struct nodeinfo_database_element elements[];
};

struct nodedata get_my_nodedata();
void print_nodedata_list(const struct nodedata_list *list);
void print_nodeinfo_database(const struct nodeinfo_database *db);
int update_nodeinfo(struct nodedata_list *list);
int update_hostfile(const struct nodedata_list *list);

// DB receive helpers
// Receive a nodeinfo_database datagram on an existing socket.
// On success: returns malloc'd pointer and sets *out_bytes to datagram size.
// On timeout: returns NULL and sets *out_bytes to IS_TIMEOUT.
// On error:   returns NULL and sets *out_bytes to -1.
struct nodeinfo_database *receive_nodeinfo_database_on_socket(int sock,
                                                             int timeout_sec,
                                                             int timeout_usec,
                                                             int *out_bytes);

// Convenience: bind a temporary UDP socket to `port`, receive one DB, then close.
// Same return contract as above.
struct nodeinfo_database *receive_nodeinfo_database_bound(int port,
                                                         int timeout_sec,
                                                         int timeout_usec,
                                                         int *out_bytes);

#endif /* COMMON_H */