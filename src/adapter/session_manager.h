#pragma once

#include "session.h"
#include "plugin_spec.h"

struct session_runtime
{
	struct session *session;
	plugin_entry_callback *entry_cb;
	int interest_state;
	void *ctx;
};

struct session
{
	struct streaminfo *stream;
	struct plugin_spec *desc;
	enum session_type type;
	enum session_state state;
	struct session_runtime *plugin_rt;
	int plugin_rt_num;
	struct session_mq *mq;
};

struct streaminfo;
struct session *session_new(struct streaminfo* stream, struct plugin_spec *desc, int desc_num, enum session_type type, int thread_id);
void session_free(struct session *session, int thread_id);

void session_dispatch(struct session *session, enum session_state state, int thread_id);