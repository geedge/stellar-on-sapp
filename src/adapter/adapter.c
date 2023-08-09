#include "adapter.h"
#include "session_manager.h"

#include "stream.h"

struct adapter
{
	int subsystem_num;
	struct subsystem_schema *desc;
};

struct adapter *adapter_init(void)
{
	struct adapter *adapter = CALLOC(struct adapter, 1);
	adapter->subsystem_num = sizeof(global_subsystem_schema) / sizeof(global_subsystem_schema[0]);
	adapter->desc = CALLOC(struct subsystem_schema, adapter->subsystem_num);
	memcpy(adapter->desc, global_subsystem_schema, sizeof(global_subsystem_schema));
	for(int i = 0; i < adapter->subsystem_num; i++)
	{
		if (adapter->desc[i].init_cb != NULL)
		{
			adapter->desc[i].init_cb();
		}
	}
	return adapter;
}

void adapter_exit(struct adapter *adapter)
{
	for(int i = 0; i < adapter->subsystem_num; i++)
	{
		if (adapter->desc[i].exit_cb != NULL)
		{
			adapter->desc[i].exit_cb();
		}
	}
	FREE(adapter->desc);
	FREE(adapter);
	return;	
};


struct streaminfo;
//streaminfo open
uint8_t adapter_session_open(struct adapter *adapter, struct streaminfo *stream, struct session **session, int thread_id)
{
	enum session_type type;
	if(stream->type==STREAM_TYPE_TCP)
	{
		type=SESSION_TYPE_TCP;
	}
	else if(stream->type==STREAM_TYPE_UDP)
	{
		type=SESSION_TYPE_UDP;
	}
	else
	{
		return -1;
	}
	*session=session_new(stream, type, SESSION_STATE_OPENING, adapter->subsystem_num, stream->threadnum);
	session_dispatch(adapter->desc, adapter->subsystem_num, *session, type, SESSION_STATE_OPENING, stream->threadnum);
	return 0;
}

uint8_t adapter_session_active(struct adapter *adapter, struct streaminfo *stream, struct session **session, int thread_id)
{
	enum session_type type;
	if(stream->type==STREAM_TYPE_TCP)
	{
		type=SESSION_TYPE_TCP;
	}
	else if(stream->type==STREAM_TYPE_UDP)
	{
		type=SESSION_TYPE_UDP;
	}
	else
	{
		return -1;
	}
	if(*session==NULL)
	{
		*session=session_new(stream, type, SESSION_STATE_ACTIVE, adapter->subsystem_num,stream->threadnum);
	}
	(*session)->state=SESSION_STATE_ACTIVE;
	session_dispatch(adapter->desc, adapter->subsystem_num, *session, type, SESSION_STATE_ACTIVE, stream->threadnum);
	return 0;
}

//streaminfo close, or firewall active close streaminfo  
uint8_t adapter_session_close(struct adapter *adapter, struct streaminfo *stream, struct session **session, int thread_id)
{
	enum session_type type;
	if(stream->type==STREAM_TYPE_TCP)
	{
		type=SESSION_TYPE_TCP;
	}
	else if(stream->type==STREAM_TYPE_UDP)
	{
		type=SESSION_TYPE_UDP;
	}
	else
	{
		return -1;
	}
	session_dispatch(adapter->desc, adapter->subsystem_num, *session, type,  SESSION_STATE_CLOSING, stream->threadnum);
	if(*session)
	{
		session_free(*session, type, adapter->subsystem_num, stream->threadnum);
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