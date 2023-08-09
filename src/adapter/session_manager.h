#pragma once

#include "session.h"
#include "subsystem_manager.h"


struct subsystem_session_runtime
{
	struct session *session;
	subsystem_entry_callback *entry_cb;
	int interest_state;
	void *ctx;
};

struct session
{
	struct streaminfo *stream;
	enum session_type type;
	enum session_state state;
	struct subsystem_session_runtime *subsystem_rt;
	int subsystem_rt_num;
	struct session_mq *mq;
};

struct streaminfo;
struct session *session_new(struct streaminfo* stream, enum session_type type, enum session_state state, int subsystem_runtime_num, int thread_id);
void session_free(struct session *session, enum session_type type, int subsystem_runtime_num, int thread_id);

struct subsystem_descriptor ;
void session_dispatch(struct subsystem_descriptor *desc, int desc_num, struct session *session, enum session_type type, enum session_state state, int thread_id);