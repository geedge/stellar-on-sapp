#pragma once

#include "session.h"

typedef int plugin_init_callback(plugin_id_t plugin_id);
typedef void plugin_exit_callback(void);
typedef void plugin_session_callback(struct session *session, enum session_state state, int thread_id);

