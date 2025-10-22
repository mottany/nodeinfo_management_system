#ifndef CTRL_SERVICE_H
#define CTRL_SERVICE_H

#include "db_types.h"

int accept_request();

enum {
    JOIN_REQUEST_CODE               = 1,
    RECV_NODEDATA_LIST_REQUEST_CODE = 2,
    NODEINFO_DB_REQUEST_CODE        = 3,
};

// Reply to the last requester (captured in accept_request) with the current DB.
// NAT を考慮し、受信に使った同じソケット/ポートから送信します。
// ロジック:
//  - db が NULL または current_size==0 の場合: ALREADY_UPTODATE_MSG を返す
//  - それ以外: SEND_YOU_DB_MSG を返した後、DB スナップショットを 1 datagram で送信
int send_nodeinfo_database(const struct nodeinfo_database *db);

#endif /* CTRL_SERVICE_H */
