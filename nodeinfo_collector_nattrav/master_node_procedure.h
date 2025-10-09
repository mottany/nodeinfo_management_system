#ifndef MASTER_NODE_PROCEDURE_H
#define MASTER_NODE_PROCEDURE_H

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>

#include "common_asset.h"

#define RELAY_SERVER_IP "
#define RELAY_SERVER_PORT 9000

#define JOIN_REQUEST_CODE 1
#define LEAVE_REQUEST_CODE 2
#define RECV_DB_REQUEST_CODE 3

int accept_request();
int receive_nodedata();
int add_nodedata_to_list();
int remove_nodedata_from_list();
int send_nodedata_list();
int request_join_huge_cluster();
int send_nodeinfo_database();
int receive_nodeinfo_database();
int run_master_node_procedure();

#endif /* MASTER_NODE_PROCEDURE_H */