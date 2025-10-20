#ifndef DB_TYPES_H
#define DB_TYPES_H

#include <stdint.h>

// Node data sent from master
struct nodedata {
    int ipaddress;     // network byte order IPv4
    int userid;
    int cpu_core_num;
};

struct nodedata_list {
    int max_size;
    int current_size;
    int network_id;    // target network id
    struct nodedata nodedatas[];
};

// Database stored on relay/db-manager
struct nodeinfo_database_element {
    int network_id;
    uint32_t ipaddress;        // network byte order IPv4
    int userid;
    int cpu_core_num;
    int control_port_num;
    int message_port_num;
};

struct nodeinfo_database {
    int max_size;
    int current_size;
    struct nodeinfo_database_element elements[];
};

#endif /* DB_TYPES_H */
