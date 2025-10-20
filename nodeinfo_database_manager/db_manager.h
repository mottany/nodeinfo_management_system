#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include "db_types.h"

// Build or update internal DB with nodes from list->network_id.
// Returns a malloc'ed snapshot of the current DB (caller must free), or NULL on error.
struct nodeinfo_database *update_nodeinfo_database(const struct nodedata_list *list);

// Pretty print a snapshot of nodeinfo_database
void print_nodeinfo_database(const struct nodeinfo_database *db);

#endif /* DB_MANAGER_H */
