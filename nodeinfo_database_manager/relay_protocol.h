#ifndef RELAY_PROTOCOL_H
#define RELAY_PROTOCOL_H

// Control messages and ports used by the db manager side
#define HELLO_RELAY_SERVER_MSG        "Hello_relay_server!"
#define READY_SEND_NODEDATA_LIST_MSG  "Ready_to_send_nodedata_list"
#define READY_RECV_NODEDATA_LIST_MSG  "Ready_to_recv_nodedata_list"

#define CTRL_MSG_PORT       8000
#define NODEDATA_LIST_PORT  8002

#endif /* RELAY_PROTOCOL_H */
