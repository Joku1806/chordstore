#ifndef DATASTORE_H
#define DATASTORE_H

#include "protocol.h"

// database-specific functions
crud_packet* execute_ds_action(crud_packet* pkg);
crud_packet* ds_query(bytebuffer* key);
void ds_set(crud_packet* pkg);
int ds_delete(bytebuffer* key);
void ds_destruct();

#endif