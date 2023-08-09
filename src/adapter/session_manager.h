#pragma once

#include "session.h"

struct session
{
	struct streaminfo *stream;
	enum session_type type;
	enum session_state state;
	void **subsystem_contexts;
	struct session_mq *mq;
};

struct streaminfo;
struct session *session_new(struct streaminfo* stream, enum session_type type, enum session_state state, int thread_id);
void session_free(struct session *session, int thread_id);

void session_dispatch(struct session *session, int thread_id);