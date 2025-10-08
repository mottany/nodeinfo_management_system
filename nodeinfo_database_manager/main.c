#include <stdio.h>
#include <stdint.h>

#define HELLO_RELAY_SERVER_MSG "Hello_relay_server!"
#define Welcome_CLUSTER_MSG "Welcome_cluster!"

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

int accept_join_request() {
    // クラスタ参加要求受信
}

int assign_network_id() {
    // ネットワークID割り当て
}

int receive_nodedata_list() {
    // ノード情報リスト受信
}

int update_nodeinfo_database() {
    // ノード情報データベース更新
}

int send_nodeinfo_database() {
    // ノード情報データベース送信
}

int main(){
    fprintf(stderr, "[+]: Start nodeinfo database manager\n");

    while(1){
        accept_join_request();
        
        assign_network_id();

        receive_nodedata_list();

        update_nodeinfo_database();

        send_nodeinfo_database();
    }

    return 0;
}