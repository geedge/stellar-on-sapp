#pragma once

#include "stellar/session.h"

#include "stellar/stellar.h"
#include "uthash/utarray.h"


struct streaminfo;
struct session_exdata_runtime;
struct session_mq;
struct packet;
struct session
{
	enum session_type type;
	enum session_state state;
	struct packet *cur_pkt;
	struct stellar *st;
	struct session_exdata_runtime *ex_data_rt;
	struct session_mq *mq;
	struct session_event *ev_list;
};

struct session *session_new(struct stellar *st, enum session_type type, int thread_id);

struct packet *session_get_current_packet(struct session *sess);
void session_set_current_packet(struct session *sess, struct packet *pkt);

void session_free(struct session *sess);

void session_defer_loop(struct session *sess);

void session_dispatch(struct session *sess, enum session_state state, struct packet *pkt);