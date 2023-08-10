#pragma once

#include "session.h"

typedef int plugin_init_callback(int plugin_id);
typedef void plugin_exit_callback(void);
typedef void plugin_entry_callback(struct session *session, enum session_state state, int thread_id);

struct plugin_spec
{
	plugin_init_callback *init_cb;
	plugin_exit_callback *exit_cb;
	plugin_entry_callback *udp_session_entry_cb;
	plugin_entry_callback *tcp_session_entry_cb;
	int udp_session_interest_state;
	int tcp_session_interest_state;
};