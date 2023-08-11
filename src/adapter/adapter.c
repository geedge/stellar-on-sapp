#include "session.h"
#include "utils.h"
#include "adapter.h"
#include "session_manager.h"

#include "uthash/utarray.h"

#include <MESA/stream.h>

UT_array *global_plugin_specs=0;
int global_plugin_runtime_exdata_idx=-1;

UT_icd plugin_specs_icd = {sizeof(struct plugin_specific), NULL, NULL, NULL};

static void exdata_plugin_rt_free(struct session *sess, int idx, void *ex_ptr, void *arg)
{
	if(ex_ptr)
	{
		utarray_free((UT_array *)ex_ptr);
	}
	return;
}

int adapter_init(struct plugin_specific specs[] , int spec_num)
{
	utarray_new(global_plugin_specs,&plugin_specs_icd);
	int init_cb_ret = 0;
	for(int i = 0; i < spec_num; i++)
	{
		if (specs[i].init_cb != NULL)
		{
			init_cb_ret=specs[i].init_cb(i);
			if(init_cb_ret < 0)
			{
				goto ADAPTER_INIT_ERROR;
			}
			utarray_push_back(global_plugin_specs, &specs[i]);
		}
	}
	global_plugin_runtime_exdata_idx=session_get_ex_new_index("__SYSTEM__", exdata_plugin_rt_free, NULL);
	return 0;
ADAPTER_INIT_ERROR:
	adapter_exit();
	return -1;
}

void adapter_exit()
{
	struct plugin_specific *p=NULL;
	while ((p = (struct plugin_specific *)utarray_next(global_plugin_specs, p)))
	{
		if(p->exit_cb != NULL)
		{
			p->exit_cb();
		}
	}
	session_mq_cleanup();
	utarray_free(global_plugin_specs);
	return;	
};


struct plugin_runtime_exdata
{
	plugin_entry_callback *session_entry_cb;
	int session_interest_state;
};

UT_icd plugin_runtime_exdata_icd = {sizeof(struct plugin_runtime_exdata), NULL, NULL, NULL};
static UT_array * adapter_init_plugin_runtime(struct session *sess, enum session_type type)
{
	UT_array *plugin_rt = NULL;
	unsigned int len = utarray_len(global_plugin_specs);
	if (len > 0)
	{
		utarray_new(plugin_rt, &plugin_runtime_exdata_icd);
		for (unsigned int i = 0; i < len; i++)
		{
			struct plugin_specific *spec = (struct plugin_specific *)utarray_eltptr(global_plugin_specs, i);
			struct plugin_runtime_exdata *exdata = CALLOC(struct plugin_runtime_exdata, 1);
			if (type == SESSION_TYPE_TCP)
			{
				exdata->session_entry_cb = spec->tcp_session_entry_cb;
				exdata->session_interest_state = spec->tcp_session_interest_state;
			}
			if (type == SESSION_TYPE_UDP)
			{
				exdata->session_entry_cb = spec->tcp_session_entry_cb;
				exdata->session_interest_state = spec->tcp_session_interest_state;
			}
			utarray_push_back(plugin_rt, exdata);
		}
	}
	return plugin_rt;
}

static void adapter_dispatch_session(int plugin_rt_exdata_idx, struct session *sess, enum session_state state, int thread_id)
{
	UT_array *plugin_rt = session_get_ex_data(sess, plugin_rt_exdata_idx);
	struct plugin_runtime_exdata *p = NULL;
	while( (p=(struct plugin_runtime_exdata*)utarray_next(plugin_rt,p))) 
	{
		if(p->session_entry_cb != NULL && (p->session_interest_state & state))
		{
			p->session_entry_cb(sess, state, thread_id);
		}
	}
	session_mq_loop(sess);
	return;
}

struct streaminfo;
//streaminfo open
void adapter_session_open(struct streaminfo *stream, struct session **sess, int thread_id)
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
		return;
	}
	*sess=session_new(stream, type, thread_id);
	UT_array *plugin_rt=adapter_init_plugin_runtime(*sess,type);
	session_set_ex_data(*sess, global_plugin_runtime_exdata_idx, plugin_rt);
	adapter_dispatch_session(global_plugin_runtime_exdata_idx, *sess, SESSION_STATE_OPENING, thread_id);
	return;
}

void adapter_session_active(struct streaminfo *stream, struct session **sess, int thread_id)
{
	if (*sess == NULL)
	{
		session_set_state(*sess, SESSION_STATE_ACTIVE, thread_id);
		adapter_dispatch_session(global_plugin_runtime_exdata_idx, *sess, SESSION_STATE_ACTIVE, thread_id);
	}
	return;
}

//streaminfo close, or firewall active close streaminfo  
void adapter_session_close(struct streaminfo *stream, struct session **sess, int thread_id)
{
	if(*sess)
	{
		session_set_state(*sess, SESSION_STATE_CLOSING, thread_id);
		adapter_dispatch_session(global_plugin_runtime_exdata_idx, *sess, SESSION_STATE_CLOSING, thread_id);
		session_free(*sess, stream->threadnum);
	}
	return;
}

void session_dettach_plugin(struct session *sess, plugin_id_t self)
{
	if(sess==NULL)return;
	UT_array *plugin_rt=session_get_ex_data(sess, global_plugin_runtime_exdata_idx);
	struct plugin_runtime_exdata *p=(struct plugin_runtime_exdata *)utarray_eltptr(plugin_rt, (unsigned int)self);
	if(p)
	{
		p->session_interest_state=SESSION_STATE_INVALID;
	}
	return;
}

void session_attach_plugin(struct session *sess, plugin_id_t self)
{	
	if(sess==NULL)return;
	UT_array *plugin_rt=session_get_ex_data(sess, global_plugin_runtime_exdata_idx);
	struct plugin_runtime_exdata *p=(struct plugin_runtime_exdata *)utarray_eltptr(plugin_rt, (unsigned int)self);
	if(p)
	{
		p->session_interest_state=SESSION_STATE_ALL;
	}
	return;
}

const char* session_get_readable_addr(struct session *sess)
{
	return printaddr(&sess->stream->addr, sess->stream->threadnum);
}

const char *session_get_current_payload(struct session *sess, size_t *payload_len)
{
	*payload_len = (size_t)sess->stream->ptcpdetail->datalen;
	return sess->stream->ptcpdetail->pdata;
}

const struct packet *session_get_current_packet(struct session *sess)
{
	return get_rawpkt_from_streaminfo(sess->stream);
}

const char *packet_get_data(const struct packet *pkt, size_t *data_len)
{
	return NULL;
}

struct flow_flag session_get_flow_flag(const struct session *sess)
{
	struct flow_flag flag={0};
	if(sess->stream)
	{
		if(sess->stream->dir==DIR_DOUBLE)
		{
			flag.flow_flag_see_client_to_server=1;
			flag.flow_flag_see_server_to_client=1;
		}
		else if(sess->stream->dir==DIR_C2S)
		{
			flag.flow_flag_see_client_to_server=1;
		}
		else if(sess->stream->dir==DIR_S2C)
		{
			flag.flow_flag_see_server_to_client=1;
		}
	}
	return flag;
}

struct flow_flag session_get_current_flow_flag(struct session *sess)
{
	struct flow_flag flag={0};
	if(sess->stream)
	{
		if(sess->stream->curdir==DIR_DOUBLE)
		{
			flag.flow_flag_see_client_to_server=1;
			flag.flow_flag_see_server_to_client=1;
		}
		else if(sess->stream->curdir==DIR_C2S)
		{
			flag.flow_flag_see_client_to_server=1;
		}
		else if(sess->stream->curdir==DIR_S2C)
		{
			flag.flow_flag_see_server_to_client=1;
		}
	}
	return flag;
}	