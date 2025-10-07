#ifndef MASTER_NODE_PROCEDURE_H
#define MASTER_NODE_PROCEDURE_H

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/time.h>

#define RELAY_SERVER_PORT 9000

int run_master_node_procedure();

#endif /* MASTER_NODE_PROCEDURE_H */