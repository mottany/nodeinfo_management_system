#ifndef RELAY_PROTOCOL_H
#define RELAY_PROTOCOL_H

// Control messages and ports used by the db manager side
#define HELLO_RELAY_SERVER_MSG          "Hello_relay_server!"
#define READY_SEND_NODEDATA_LIST_MSG    "Ready_to_send_nodedata_list"
#define READY_RECV_NODEDATA_LIST_MSG    "Ready_to_recv_nodedata_list"
#define GIVE_ME_DB_MSG                  "Give_me_db"
#define ALREADY_UPTODATE_MSG            "Already_uptodate"
#define SEND_YOU_DB_MSG                 "Send_you_db"
#define CTRL_MSG_PORT       8000
#define NODEDATA_LIST_PORT  8002
#define NODEINFO_DB_PORT    8003

#endif /* RELAY_PROTOCOL_H */
