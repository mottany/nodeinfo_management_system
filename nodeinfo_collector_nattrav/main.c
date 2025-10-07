#include "member_node_procedure.h"
#include "master_node_procedure.h"

int main(void){
    int rc_member = run_member_node_procedure();
    
    // タイムアウトの場合はマスターノードとして再起動
    if (rc_member == IS_TIMEOUT) {
        int rc_master = run_master_node_procedure();
    } else if (rc_member < 0) {
        perror("[-]: Error in node procedure");
        return -1;
    }
    
    return 0;
};