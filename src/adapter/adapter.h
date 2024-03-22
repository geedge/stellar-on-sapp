#pragma once

#include "stellar/session.h"

struct streaminfo;


struct stellar *stellar_init(const char *toml_conf_path);
void stellar_exit(struct stellar *st);

//streaminfo open
struct session *adapter_session_new(struct stellar *st, struct streaminfo *stream);
void adapter_session_free(struct session *sess);

unsigned char adapter_session_state_update(struct streaminfo *stream, struct session *sess, void *a_packet, enum session_state state);

void adapter_session_poll(struct session *sess);

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

void adapter_on_packet_update(struct stellar *st, struct streaminfo *pstream, void *pkt_hdr, enum packet_type type);
int adapter_on_polling(struct stellar *st);