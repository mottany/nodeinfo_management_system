#ifndef COMMON_FORMAT_H
#define COMMON_FORMAT_H

#define HELLO_CLUSTER_MSG "Hello_cluster!"
#define WELCOME_NODE_MSG "Welcome_node!"
#define BYE_CLUSTER_MSG "Bye_cluster"
#define BYE_NODE_MSG "Bye_node"

#define BROADCAST_PORT 8000
#define NODEDATA_PORT 8001
#define NODEDATA_LIST_PORT 8002

#include <stdint.h>

struct nodedata {
    int ipaddress;
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

#endif /* COMMON_FORMAT_H */