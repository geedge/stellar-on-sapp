#include "stellar/session.h"
#include "stellar/stellar.h"
#include "stellar/utils.h"
#include "stellar/session_exdata.h"

#include "stellar_internal.h"
#include "adapter.h"
#include "session_manager.h"

#include "uthash/utarray.h"

#include <MESA/stream.h>


inline void adapter_session_poll(struct session *sess)
{
	session_defer_loop(sess);
}

#include <dlfcn.h>
#include "toml/toml.h"

struct plugin_specific
{
	plugin_init_callback *init_cb;
	plugin_exit_callback *exit_cb;
};

struct plugin_specific_inernal
{
	struct plugin_specific specs;
	void *plugin_ctx;
};

UT_icd plugin_specs_icd = {sizeof(struct plugin_specific_inernal), NULL, NULL, NULL};

static void exdata_plugin_rt_free(struct session *sess, int idx, void *ex_ptr, void *arg)
{
	if(ex_ptr)
	{
		FREE(ex_ptr);
	}
	return;
}

static struct plugin_specific *load_plugin_specs(const char *toml_conf_path, int *spec_num)
{
	*spec_num = 0;
    FILE* fp = fopen(toml_conf_path, "r");
    char errbuf[256];
    toml_table_t* conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);	
    if (!conf) {
        fprintf(stderr, "Error parsing toml: %s\n", errbuf);
        return NULL;
    }	
    toml_array_t* plugin_array = toml_array_in(conf, "plugin");
    *spec_num = toml_array_nelem(plugin_array);
     struct plugin_specific* plugins = CALLOC(struct plugin_specific, *spec_num);

    for (int i = 0; i < *spec_num; i++) {
        toml_table_t* plugin = toml_table_at(plugin_array, i);

        const char *path_raw = toml_raw_in(plugin, "path");
        const char *init_func_name_raw = toml_raw_in(plugin, "init");
        const char *exit_func_name_raw = toml_raw_in(plugin, "exit");
        char *path = NULL;
        char *init_func_name = NULL;
        char *exit_func_name = NULL;
        if (toml_rtos(path_raw, &path) || toml_rtos(init_func_name_raw, &init_func_name) ||
            toml_rtos(exit_func_name_raw, &exit_func_name))
        {
            goto LOAD_PLUGIN_SPEC_ERROR;
        }
        void* handle = dlopen(path, RTLD_NOW|RTLD_LAZY|RTLD_LOCAL);
        if (!handle) {
            fprintf(stderr, "Error loading plugin %s: %s\n", path, dlerror());
            goto LOAD_PLUGIN_SPEC_ERROR;
        }

        plugins[i].init_cb = (plugin_init_callback *) dlsym(handle, init_func_name);
        if (!plugins[i].init_cb) {
            fprintf(stderr, "Could not load init function %s: %s\n", init_func_name, dlerror());
        }

        plugins[i].exit_cb = (plugin_exit_callback *) dlsym(handle, exit_func_name);
        if (!plugins[i].exit_cb) {
            fprintf(stderr, "Could not load exit function %s: %s\n", exit_func_name, dlerror());
        }
        FREE(path);
        FREE(init_func_name);
        FREE(exit_func_name);
    }
    toml_free(conf);
    return plugins;	
LOAD_PLUGIN_SPEC_ERROR:
	toml_free(conf);
	FREE(plugins);
	return NULL;	
}

struct stellar *stellar_init(const char *toml_conf_path)
{
	int spec_num;
	struct plugin_specific *specs = load_plugin_specs(toml_conf_path, &spec_num);
	if(specs == NULL || spec_num == 0)
	{
		return NULL;
	}
	struct stellar *st = CALLOC(struct stellar, 1);
	struct plugin_specific_inernal spec_internal={};
	utarray_new(st->plugin_specs_array,&plugin_specs_icd);
	utarray_reserve(st->plugin_specs_array, spec_num);
	st->intrinsic_session_event_exdata_idx=stellar_session_get_ex_new_index(st, "__STELLAR__", exdata_plugin_rt_free, NULL);
	for(int i = 0; i < spec_num; i++)
	{
		if (specs[i].init_cb != NULL)
		{
			spec_internal.plugin_ctx=specs[i].init_cb(st);
			if(spec_internal.plugin_ctx == NULL)
			{
				goto ADAPTER_INIT_ERROR;
			}
			spec_internal.specs=specs[i];
			utarray_push_back(st->plugin_specs_array, &spec_internal);
		}
	}
	FREE(specs);
	return st;
ADAPTER_INIT_ERROR:
	stellar_exit(st);
	FREE(specs);
	return NULL;
}

void stellar_exit(struct stellar *st)
{
	struct plugin_specific_inernal *p=NULL;
	while ((p = (struct plugin_specific_inernal *)utarray_next(st->plugin_specs_array, p)))
	{
		if(p->specs.exit_cb != NULL)
		{
			p->specs.exit_cb(p->plugin_ctx);
		}
	}
	utarray_free(st->session_mq_schema_array);
	utarray_free(st->session_exdata_schema_array);
	utarray_free(st->intrinsic_session_event_schema_array);
	utarray_free(st->plugin_specs_array);
	FREE(st);
	return;	
};

struct streaminfo;
//streaminfo open
struct session *adapter_session_open(struct stellar *st, struct streaminfo *stream, void *a_packet)
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
		return NULL;
	}
	struct session *sess=session_new(st, stream, type, stream->threadnum);
	session_dispatch(sess, SESSION_STATE_OPENING, a_packet);
	return sess;
}

void adapter_session_active(struct streaminfo *stream, struct session *sess, void *a_packet)
{
	if (sess)
	{
		session_dispatch( sess, SESSION_STATE_ACTIVE, a_packet);
	}
	return;
}

//streaminfo close, or firewall active close streaminfo  
void adapter_session_close(struct streaminfo *stream, struct session *sess, void *a_packet)
{
	if(sess)
	{
		session_dispatch(sess, SESSION_STATE_CLOSING, a_packet);
		session_free(sess);
	}
	return;
}

const char* session_get0_readable_addr(struct session *sess)
{
	return printaddr(&sess->stream->addr, sess->stream->threadnum);
}

const char *session_get0_current_payload(struct session *sess, size_t *payload_len)
{
	*payload_len = (size_t)sess->stream->ptcpdetail->datalen;
	return (const char*)sess->stream->ptcpdetail->pdata;
}

const struct packet *session_get0_current_packet(struct session *sess)
{
	return (const struct packet *)get_rawpkt_from_streaminfo(sess->stream);
}

int packet_get_direction(const struct packet *pkt)
{
	// TODO: 
	return -1;
}   	

const char *packet_get0_data(const struct packet *pkt, size_t *data_len)
{
	// TODO:
	return NULL;
}

int  session_get_direction(const struct session *sess)
{
	// TODO: check route dir
	if(sess->stream)
	{
		if(sess->stream->curdir==DIR_C2S)
		{
			return SESSION_DIRECTION_IN;
		}
		else if(sess->stream->curdir==DIR_S2C)
		{
			return SESSION_DIRECTION_OUT;
		}
	}
	return -1;
} 

int session_is_symmetric(const struct session *sess, unsigned char *flag)
{
	int is_symmetric=0;
	if(sess->stream)
	{
		if(sess->stream->curdir==DIR_DOUBLE)
		{
			*flag=(SESSION_SEEN_C2S_FLOW|SESSION_SEEN_S2C_FLOW);
			is_symmetric=1;
		}
		else if(sess->stream->curdir==DIR_C2S)
		{
			*flag=SESSION_SEEN_C2S_FLOW;
		}
		else if(sess->stream->curdir==DIR_S2C)
		{
			*flag=SESSION_SEEN_S2C_FLOW;
		}
	}
	return is_symmetric;
}	

int stellar_get_worker_thread_num(struct stellar *st)
{
	return get_thread_count();
}

int stellar_get_current_thread_id(struct stellar *st)
{
	// TODO:
	return 0;
}