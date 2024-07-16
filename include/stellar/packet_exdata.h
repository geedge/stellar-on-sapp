#pragma once

#include "stellar.h"

typedef void packet_exdata_free(struct packet *pkt, int idx, void *ex_ptr, void *arg);
int stellar_packet_exdata_new_index(struct stellar *st, const char *name, packet_exdata_free *free_func,void *arg); 
int packet_exdata_set(struct packet *pkt, int idx, void *ex_ptr);
void *packet_exdata_get(struct packet *pkt, int idx);