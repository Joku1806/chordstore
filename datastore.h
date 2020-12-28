#ifndef DATASTORE_H
#define DATASTORE_H

#include "crud_protocol.h"

// database-specific functions
hash_packet* execute_ds_action(hash_packet* pkg);
hash_packet* ds_query(bytebuffer* key);
void ds_set(hash_packet* pkg);
int ds_delete(bytebuffer* key);
void ds_destruct();

#endif