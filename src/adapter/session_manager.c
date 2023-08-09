#include "session.h"
#include "utils.h"
#include "session_manager.h"
#include "subsystem_manager.h"
#include "session_mq_internal.h"

#include "uthash/uthash.h"

struct streaminfo;
struct session *session_new(struct streaminfo* stream, enum session_type type, enum session_state state, int subsystem_runtime_num, int thread_id)
{
	if(stream == NULL)return NULL;
	struct session *session = CALLOC(struct session, 1);
	session->stream = stream;
	session->type = type;
	session->state = state;
	session->subsystem_rt = CALLOC(struct subsystem_session_runtime, subsystem_runtime_num);
	session->mq = session_mq_new();
	return session;

}

void session_free(struct session *session,  enum session_type type, int subsystem_runtime_num, int thread_id)
{
	if(session==NULL)return;
	
	if(session->subsystem_rt != NULL)
	{
		FREE(session->subsystem_rt);
	}
	if(session->mq != NULL)
	{
		session_mq_free(session->mq);
	}
}

void session_dispatch(struct subsystem_schema *desc, int desc_num, struct session *session, enum session_type type, enum session_state state, int thread_id)
{
	if(session==NULL)return;

	int interest_state=0;
	subsystem_entry_callback *entry_cb=NULL;

	for (int i = 0; i < session->subsystem_rt_num; i++)
	{
		switch (state)
		{
		case SESSION_STATE_OPENING:
			if (type == SESSION_TYPE_TCP)
			{
				interest_state = desc[i].tcp_session_interest_state;
				entry_cb = desc[i].tcp_session_entry_cb;
			}
			if (type == SESSION_TYPE_UDP)
			{
				interest_state = desc[i].udp_session_interest_state;
				entry_cb = desc[i].udp_session_entry_cb;
			}
			session->subsystem_rt[i].session = session;
			session->subsystem_rt[i].entry_cb = entry_cb;
			session->subsystem_rt[i].interest_state = interest_state;
			break;
		case SESSION_STATE_ACTIVE:
		case SESSION_STATE_CLOSING:
			interest_state = session->subsystem_rt[i].interest_state;
			entry_cb = session->subsystem_rt[i].entry_cb;
			break;

		default:
			break;
		}
		if ((interest_state & session->state) && (entry_cb != NULL))
		{
			entry_cb(session, session->state, thread_id, &((session->subsystem_rt + i)->ctx));
		}
	}
	session_mq_dequeue(session->mq, session);
	return;
}

