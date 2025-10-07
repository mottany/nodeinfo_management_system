#ifndef MEMBER_NODE_PROCEDURE_H
#define MEMBER_NODE_PROCEDURE_H

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/time.h>

#define TIMEOUT_SEC 10
#define TIMEOUT_USEC 0
#define IS_TIMEOUT -2

int request_join_cluster(struct sockaddr_in *master_node_addr);
int send_my_nodedata(struct sockaddr_in *master_node_addr);
int receive_nodedata_list(struct nodedata_list *list);
int update_nodeinfo(struct nodedata_list *list);
int update_hostfile(struct nodedata_list *list);
int run_member_node_procedure();

#endif /* MEMBER_NODE_PROCEDURE_H */