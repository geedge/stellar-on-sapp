#pragma once

#include "stellar/stellar.h"

struct plugin_manager_schema;
struct plugin_manager_runtime;


struct stellar * packet_stellar_get(struct packet *pkt);

int stellar_plugin_manager_schema_set(struct stellar *st, struct plugin_manager_schema *pm);
struct plugin_manager_schema * stellar_plugin_manager_schema_get(struct stellar *st);
struct plugin_manager_runtime * session_plugin_manager_runtime_get(struct session *sess);


enum packet_type
{
	UNKNOWN,
	IPv4,
	IPv6,
	UDP,
	TCP,
	TCP_STREAM,
	CONTROL,
};

enum packet_type packet_get_type(const struct packet *pkt);