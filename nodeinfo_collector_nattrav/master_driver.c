#include "master_node_procedure.h"
#include "common_asset.h"
#include <stdio.h>
#include <arpa/inet.h>

void drive_receive_nodedata(){
    if(receive_nodedata() == 0){
        fprintf(stderr, "[+]: Successfully received nodedata from member node\n");
    } else {
        fprintf(stderr, "[-]: Failed to receive nodedata from member node\n");
    }
};

int main() {
    drive_receive_nodedata();

    return 0;
}