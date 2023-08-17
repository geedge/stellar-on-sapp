#pragma once

#include "stellar/session.h"

#include "stellar/stellar.h"
#include "uthash/utarray.h"


struct streaminfo;
struct session_exdata_runtime;
struct session_mq;
struct session
{
	enum session_type type;
	enum session_state state;
	struct streaminfo *stream;
	struct stellar *st;
	struct session_exdata_runtime *ex_data_rt;
	struct session_mq *mq;
	struct session_event *ev_list;
};

struct session *session_new(struct stellar *st, struct streaminfo* stream, enum session_type type, int thread_id);
void session_free(struct session *sess);

void session_defer_loop(struct session *sess);

void session_dispatch(struct session *sess, enum session_state state, struct packet *pkt);