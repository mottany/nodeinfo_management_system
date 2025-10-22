// Keep only main
#include <stdio.h>
#include <stdlib.h>

#include "ctrl_service.h"
#include "nodedata_receiver.h"
#include "db_types.h"
#include "db_manager.h"

int main(void) {
    fprintf(stderr, "[+]: Start nodeinfo database manager\n");

    while (1) {
        int request_code = accept_request();
        if (request_code < 0) {
            fprintf(stderr, "[-]: Error in accepting join request\n");
            continue;
        }

        if (request_code == JOIN_REQUEST_CODE) {
            continue;
        } else if (request_code == RECV_NODEDATA_LIST_REQUEST_CODE) {
            // ノード情報リスト受信要求
            struct nodedata_list *list = receive_nodedata_list();
            if (!list) {
                fprintf(stderr, "[-]: Error in receiving nodedata list\n");
                continue;
            }
            struct nodeinfo_database *db = update_nodeinfo_database(list);
            free(list);
            if (!db) {
                fprintf(stderr, "[-]: Error in updating nodeinfo database\n");
                continue;
            }
            print_nodeinfo_database(db);
            // TODO: send_nodeinfo_database(db);
            free(db);
        }
    }

    return 0;
}