#ifndef MASTER_NODE_PROCEDURE_H
#define MASTER_NODE_PROCEDURE_H

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/time.h>

#define RELAY_SERVER_IP "
#define RELAY_SERVER_PORT 9000

int accept_join_request();
int receive_nodedata();
int add_nodedata_to_list();
int remove_nodedata_from_list();
int send_nodedata_list();
int request_join_huge_cluster();
int receive_nodeinfo_database();
int run_master_node_procedure();

#endif /* MASTER_NODE_PROCEDURE_H */