//subsystem manager
#pragma once

#include "session.h"

typedef int subsystem_init_callback(void);
typedef void subsystem_exit_callback(void);
typedef void subsystem_entry_callback(struct session *session, enum session_state state, int thread_id, void **ctx);

struct subsystem_schema;
struct subsystem_schema *subsystem_get0_schema(enum session_type type, subsystem_entry_callback *entry_cb);
void *subsystem_session_ctx_get(struct session *session, struct subsystem_schema *schema);	

void subsystem_dettach_session(struct session *session,  struct subsystem_schema *schema);
void subsystem_attach_session(struct session *session,  struct subsystem_schema *schema);