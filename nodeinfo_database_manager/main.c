#include <stdio.h>
#include <stdint.h>

#define HELLO_RELAY_SERVER_MSG "Hello_relay_server!"

int network_id = 1;

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

int main(){
    
    // クラスタ参加要求受信
    accept_join_request();
    
    // クラスタ参加応答送信（ID付与）
    assign_network_id();

    receive_nodedata_list();

    update_nodeinfo_database();

    send_nodeinfo_database();

    close();

    return 0;
}