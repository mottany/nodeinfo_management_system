#include "member_node_procedure.h"
#include "common_asset.h"
#include <stdio.h>
#include <arpa/inet.h>

void drive_send_my_nodedata(){
    struct sockaddr_in master_node_addr;
    master_node_addr.sin_family = AF_INET;
    master_node_addr.sin_port = htons(8000); // Example port
    inet_pton(AF_INET, "192.168.1.1", &master_node_addr.sin_addr); // Example IP
    if(send_my_nodedata(&master_node_addr) == 0){
        fprintf(stderr, "[+]: Successfully sent my nodedata to master node\n");
    } else {
        fprintf(stderr, "[-]: Failed to send my nodedata to master node\n");
    }
};

int main() {
    drive_send_my_nodedata();

    return 0;
}