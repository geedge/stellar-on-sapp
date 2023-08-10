#include "utils.h"
#include "adapter.h"
#include "session_manager.h"

#include <MESA/stream.h>

struct adapter
{
	int plugin_spec_num;
	int init_succ_plugin_num;
	struct plugin_spec *specs;
	int *plugin_init_succ;
};

struct adapter *adapter_init(struct plugin_spec specs[] , int spec_num)
{
	struct adapter *adapter = CALLOC(struct adapter, 1);
	adapter->plugin_spec_num=spec_num;
	adapter->init_succ_plugin_num=0;
	adapter->specs = CALLOC(struct plugin_spec, spec_num);
	memcpy(adapter->specs, specs, sizeof(struct plugin_spec) * spec_num);	
	adapter->plugin_init_succ=CALLOC(int, spec_num);
	int init_cb_ret = 0;
	for(int i = 0; i < adapter->plugin_spec_num; i++)
	{
		if (adapter->specs[i].init_cb != NULL)
		{
			init_cb_ret=adapter->specs[i].init_cb(i);
			if(init_cb_ret < 0)
			{
				adapter->specs[i].tcp_session_interest_state=SESSION_STATE_INVALID;
				adapter->specs[i].udp_session_interest_state=SESSION_STATE_INVALID;
			}
			else
			{
				adapter->init_succ_plugin_num++;
				adapter->plugin_init_succ[i]=1;
			}
		}
	}
	return adapter;
}

void adapter_exit(struct adapter *adapter)
{
	for(int i = 0; i < adapter->plugin_spec_num; i++)
	{
		if ( adapter->plugin_init_succ[i]==1 && adapter->specs[i].exit_cb != NULL)
		{
			adapter->specs[i].exit_cb();
		}
	}
	FREE(adapter->plugin_init_succ);
	FREE(adapter->specs);
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
	*session=session_new(stream, adapter->specs, adapter->plugin_spec_num, type, thread_id);
	session_dispatch(*session, SESSION_STATE_ACTIVE, thread_id);
	return 0;
}

uint8_t adapter_session_active(struct adapter *adapter, struct streaminfo *stream, struct session **session, int thread_id)
{
	if(*session==NULL)
	{
		return -1;
	}
	(*session)->state=SESSION_STATE_ACTIVE;
	session_dispatch(*session, SESSION_STATE_ACTIVE, thread_id);
	return 0;
}

//streaminfo close, or firewall active close streaminfo  
uint8_t adapter_session_close(struct adapter *adapter, struct streaminfo *stream, struct session **session, int thread_id)
{
	if(*session)
	{
		session_dispatch(*session, SESSION_STATE_CLOSING, thread_id);
		session_free(*session, stream->threadnum);
	}
	return 0;
}

uint8_t session_direction_get(struct session *session)
{
	switch(session->stream->dir)	
	{
		case DIR_C2S:
			return SESSION_DIR_C2S;
		case DIR_S2C:
			return SESSION_DIR_S2C;
		case DIR_DOUBLE:
			return SESSION_DIR_DOUBLE;
		default:
			break;
	}
	return 0;
}      
uint8_t session_current_direction_get(struct session *session)
{
	switch(session->stream->curdir)	
	{
		case DIR_C2S:
			return SESSION_DIR_C2S;
		case DIR_S2C:
			return SESSION_DIR_S2C;
		case DIR_DOUBLE:
			return SESSION_DIR_DOUBLE;
		default:
			break;
	}
	return 0;
}   

const char* session_printaddr(struct session *session)
{
	return printaddr(&session->stream->addr, session->stream->threadnum);
}

const char *session_payload_get(struct session *session, int *len)
{
	*len = session->stream->ptcpdetail->datalen;
	return session->stream->ptcpdetail->pdata;
}