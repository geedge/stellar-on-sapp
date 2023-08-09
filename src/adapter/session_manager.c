#include "utils.h"
#include "session_manager.h"
#include "subsystem_manager.h"
#include "session_mq_internal.h"

#include "uthash/uthash.h"

struct streaminfo;
struct session *session_new(struct streaminfo* stream, enum session_type type, enum session_state state, int thread_id)
{
	if(stream == NULL)return NULL;
	struct session *session = CALLOC(struct session, 1);
	session->stream = stream;
	session->type = type;
	session->state = state;
	session->subsystem_contexts = NULL;
	session->mq = session_mq_new();
	return session;

}

void session_free(struct session *session, int thread_id)
{
	if(session==NULL)return;

	if(session->subsystem_contexts != NULL)
	{
		FREE(session->subsystem_contexts);
	}
	if(session->mq != NULL)
	{
		session_mq_free(session->mq);
	}
}

void session_dispatch(struct session *session, int thread_id)
{
	if(session==NULL)return;
	int subsystem_num = sizeof(global_subsystem_schema) / sizeof(global_subsystem_schema[0]);
	for(int i = 0; i < subsystem_num; i++)
	{
		if ((global_subsystem_schema[i].interest_state & session->state) && (global_subsystem_schema[i].entry_cb != NULL))
		{
			global_subsystem_schema[i].entry_cb(session, session->state, thread_id, session->subsystem_contexts[i]);
		}
	}
	session_mq_dequeue(session->mq, session);
	return;
}

