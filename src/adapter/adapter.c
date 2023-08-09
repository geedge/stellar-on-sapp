#include "adapter.h"
#include "session_manager.h"

#include "stream.h"

struct adapter
{

};

struct adapter *adapter_init(void)
{
	return NULL;
}

void adapter_exit(struct adapter *adapter)
{
	return;
};


struct streaminfo;
//streaminfo open
uint8_t adapter_session_open(struct streaminfo *stream, struct session **session, int thread_id)
{
	*session=session_new(stream, SESSION_TYPE_TCP, SESSION_STATE_OPENING, stream->threadnum);
	session_dispatch(*session, stream->threadnum);
	return 0;
}

uint8_t adapter_session_active(struct streaminfo *stream, struct session **session, int thread_id)
{
	if(*session==NULL)
	{
		*session=session_new(stream, SESSION_TYPE_TCP, SESSION_STATE_ACTIVE, stream->threadnum);
	}
	(*session)->state=SESSION_STATE_ACTIVE;
	session_dispatch(*session, stream->threadnum);
	return 0;
}

//streaminfo close, or firewall active close streaminfo  
uint8_t adapter_session_close(struct streaminfo *stream, struct session **session, int thread_id)
{
	session_dispatch(*session, stream->threadnum);
	if(*session)
	{
		session_free(*session, stream->threadnum);
	}
	return 0;
}

uint8_t session_direction_get(struct session *session)
{
	return 0;
}      
uint8_t session_current_direction_get(struct session *session)
{
	return 0;
}   