#pragma once

#include "session.h"
#include "plugin_spec.h"

#include "uthash/utarray.h"


struct streaminfo;
struct session
{
	enum session_type type;
	enum session_state state;
	struct streaminfo *stream;
	struct session_mq *mq;
	UT_array *ex_data_rt;
};

struct session *session_new(struct streaminfo* stream, enum session_type type, int thread_id);
void session_free(struct session *session, int thread_id);

void session_set_state(struct session *session, enum session_state state, int thread_id);

void session_mq_loop(struct session *session);
void session_mq_cleanup();