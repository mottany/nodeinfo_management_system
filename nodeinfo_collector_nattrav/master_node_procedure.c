#include "master_node_procedure.h"

int accept_request() {
    // ノード登録/脱退要求とデータベース受け渡し要求の受諾
}

int receive_nodedata() {
    // ノード情報受信
}

int add_nodedata_to_list() {
    // 受信したノード情報をリストに追加
}

int remove_nodedata_from_list() {
    // 受信したノード情報をリストから削除
}

int send_nodedata_list() {
    // ノード情報リスト送信
}

int request_join_huge_cluster() {
    // 中継サーバにクラスタ参加要求
}

int receive_nodeinfo_database() {
    // 中継サーバからノード情報データベース受信
}

int send_nodeinfo_database() {
    // メンバノードにノード情報データベース送信
}

int run_master_node_procedure() {
    fprintf(stderr, "[+]: Start master node procedure\n");

    while(1){
        // メンバノードと中継サーバから要求メッセージを受け入れる
        int request_code = accept_request();
        printf("%d\n", request_code);
        /*
        // メンバノードからノード登録要求を受信したら
        if(request_code == JOIN_REQUEST_CODE){
            receive_nodedata();
            add_nodedata_to_list();
            send_nodedata_list();   // メンバノードと中継サーバの両方にnodedata_listを送る。
        }
        // メンバノードからノード脱退要求を受信したら
        else if(request_code == LEAVE_REQUEST_CODE){
            receive_nodedata();
            remove_nodedata_from_list();
            send_nodedata_list();   // メンバノードと中継サーバの両方にnodedata_listを送る。
        }
        // 中継サーバから「データベースを受け取れ」要求を受信したら
        else if(request_code == READY_DB_CODE){
            receive_nodeinfo_database();
            send_nodeinfo_database();
        }*/
    }
    
    return 0;
};