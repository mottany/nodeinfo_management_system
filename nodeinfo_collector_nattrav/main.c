#include "member_node_procedure.h"
#include "master_node_procedure.h"

int main(void){
    int rc = run_member_node_procedure();
    
    // タイムアウトの場合はマスターノードとして再起動
    if (rc == IS_TIMEOUT) {
        if (run_master_node_procedure() < 0) {
            return -1;
        }
    } else if (rc < 0) {
        perror("[-]: Error in node procedure");
        return -1;
    }
    
    return 0;
};