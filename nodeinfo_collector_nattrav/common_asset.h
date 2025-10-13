#ifndef COMMON_ASSET_H
#define COMMON_ASSET_H

#include <stdint.h>

#define HELLO_CLUSTER_MSG "Hello_cluster!"
#define WELCOME_NODE_MSG "Welcome_node!"
#define BYE_CLUSTER_MSG "Bye_cluster"
#define BYE_NODE_MSG "Bye_node"
#define READY_SEND_DB_MSG "Ready_to_send_DB"
#define READY_RECV_DB_MSG "Ready_to_recv_DB"

#define CTRL_MSG_PORT 8000
#define NODEDATA_PORT 8001
#define NODEDATA_LIST_PORT 8002

struct nodedata {
    uint32_t ipaddress;
    int userid;
    int cpu_core_num;
};

struct nodedata_list {
    int max_size;
    int current_size;
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

int update_nodeinfo();
int update_hostfile();
int receive_nodeinfo_database();

#endif /* COMMON_ASSET_H */