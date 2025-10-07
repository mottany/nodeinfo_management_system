#include "master_node_procedure.h"
#include "sock_wrapper_functions.h"
#include "common_format.h"

int accept_join_request() {
    // ノード登録要求受信
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

int run_master_node_procedure(){
    perror("[+]: Start master node procedure");
    
    // ノード登録要求又はノード脱退要求の受信
    
    // ノード登録要求を受信したら
    if(){
        send(); // ノード登録応答送信
        receive_nodedata();
        add_nodedata_to_list();
    }
    // ノード脱退要求を受信したら
    else if(){
        send(); // ノード脱退応答送信
        receive_nodedata();
        remove_nodedata_from_list();
    }

    /*メンバノードと中継サーバの両方にnodedataを送る。*/
    send_nodedata_list();
};