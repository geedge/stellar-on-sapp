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
	if(plugin_array==NULL)return NULL;
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
        void* handle = dlopen(path, RTLD_NOW|RTLD_LAZY|RTLD_GLOBAL);
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
	if(spec_num < 0)
	{
		return NULL;
	}
	struct stellar *st = CALLOC(struct stellar, 1);
	struct plugin_specific_inernal spec_internal={};
	if(spec_num > 0)
	{
		utarray_new(st->plugin_specs_array,&plugin_specs_icd);
		utarray_reserve(st->plugin_specs_array, spec_num);
	}
	st->intrinsic_session_event_exdata_idx=stellar_session_get_ex_new_index(st, "__STELLAR__", exdata_plugin_rt_free, NULL);
	for(int i = 0; i < spec_num; i++)
	{
		if (specs[i].init_cb != NULL)
		{
			spec_internal.plugin_ctx=specs[i].init_cb(st);
			spec_internal.specs=specs[i];
			utarray_push_back(st->plugin_specs_array, &spec_internal);
		}
	}
	FREE(specs);
	return st;
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


struct packet
{
	void *raw_pkt;
	struct streaminfo *a_stream;
};

static void adapter_packet_free(struct session *sess, struct packet *pkt, void *arg)
{
	if(pkt)
	{
		FREE(pkt);
	}
	return;
}

struct session *adapter_session_new(struct stellar *st, struct streaminfo *stream)
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
	struct session *sess = session_new(st, type, stream->threadnum);
	if(sess)
	{
    	struct packet *pkt = CALLOC(struct packet, 1);
    	session_set_current_packet(sess, pkt,adapter_packet_free, NULL);
    	pkt->a_stream = stream;
	}
	return sess;
}

void adapter_session_free(struct session *sess)
{
	return session_free(sess);
}

unsigned char adapter_session_state_update(struct streaminfo *stream, struct session *sess, void *a_packet, enum session_state state)
{
	struct packet *pkt = NULL;
	if(sess)
	{
		pkt=(struct packet *)session_get0_current_packet(sess);
		pkt->raw_pkt=(void *)get_current_rawpkt_from_streaminfo(stream);
		session_dispatch( sess, state, pkt);
		if(sess->set_detach_others==1)
		{
			return APP_STATE_GIVEME|APP_STATE_KILL_OTHER;
		}
		else
		{
			return APP_STATE_GIVEME;
		}
	}
	else
		return APP_STATE_DROPME;
}

const char* session_get0_readable_addr(struct session *sess)
{
	return printaddr(&sess->cur_pkt->a_stream->addr, sess->cur_pkt->a_stream->threadnum);
}

struct session_addr *session_get0_addr(struct session *sess, enum session_addr_type *addr_type)
{
	struct session_addr *addr=NULL;
	*addr_type=SESSION_ADDR_TYPE_UNKNOWN;
	if(sess && sess->cur_pkt && sess->cur_pkt->a_stream)
	{
		struct streaminfo *pstream=sess->cur_pkt->a_stream;
		if(pstream->addr.addrtype==ADDR_TYPE_IPV4)
		{
			if(pstream->type == STREAM_TYPE_TCP)
			{
				*addr_type=SESSION_ADDR_TYPE_IPV4_TCP;
			}
			else if(pstream->type == STREAM_TYPE_UDP)
			{
				*addr_type=SESSION_ADDR_TYPE_IPV4_UDP;
			}
			addr=(struct session_addr *)pstream->addr.tuple4_v4;
		}
		if(pstream->addr.addrtype==ADDR_TYPE_IPV6)
		{
			if(pstream->type == STREAM_TYPE_TCP)
			{
				*addr_type=SESSION_ADDR_TYPE_IPV6_TCP;
			}
			else if(pstream->type == STREAM_TYPE_UDP)
			{
				*addr_type=SESSION_ADDR_TYPE_IPV6_UDP;
			}
			addr=(struct session_addr *)pstream->addr.tuple4_v6;
		}		
	}
	return addr;
}

const char *session_get0_current_payload(struct session *sess, size_t *payload_len)
{
	if(sess->cur_pkt->raw_pkt == NULL || sess->cur_pkt->a_stream == NULL || sess->cur_pkt->a_stream->ptcpdetail == NULL)return NULL;
	*payload_len = (size_t)sess->cur_pkt->a_stream->ptcpdetail->datalen;
	return (const char*)sess->cur_pkt->a_stream->ptcpdetail->pdata;
}

#include <assert.h>
const struct packet *session_get0_current_packet(struct session *sess)
{
	if(sess->cur_pkt == NULL || sess->cur_pkt->a_stream == NULL)return NULL;
	sess->cur_pkt->raw_pkt=(void *)get_current_rawpkt_from_streaminfo(sess->cur_pkt->a_stream);
	return sess->cur_pkt;
}

int packet_get_direction(const struct packet *pkt)
{
	const struct streaminfo *pstream=pkt->a_stream;
	if(pstream->curdir==DIR_C2S)
	{
		return PACKET_DIRECTION_C2S;
	}
	else if(pstream->curdir==DIR_S2C)
	{
		return PACKET_DIRECTION_S2C;
	}
	return -1;
}   	

const char *packet_get0_data(const struct packet *pkt, size_t *data_len)
{
	const void *raw_data=NULL;
	int raw_datalen=0;
	int ret1 = get_opt_from_rawpkt(pkt->raw_pkt, RAW_PKT_GET_DATA, &raw_data);
	int ret2 = get_opt_from_rawpkt(pkt->raw_pkt, RAW_PKT_GET_TOT_LEN, &raw_datalen);
	if(ret1 >= 0 || ret2 >= 0)
	{
		*data_len = raw_datalen;
		return (const char *)raw_data;
	}
	else
	{
		*data_len = 0;
		return NULL;
	}
}

int  session_get_direction(struct session *sess)
{
	if(sess->cur_pkt && sess->cur_pkt->a_stream)
	{
		//int c2s_router_dir=(sess->cur_pkt->a_stream->curdir==DIR_C2S)?(sess->cur_pkt->a_stream->routedir):((sess->cur_pkt->a_stream->routedir)^1);
		u_int8_t c2s_router_dir=0;
		int dir_len=sizeof(c2s_router_dir);
		MESA_get_stream_opt(sess->cur_pkt->a_stream, MSO_STREAM_C2S_ROUTE_DIRECTION, (void *)&c2s_router_dir, &dir_len); 
		int dir=MESA_dir_link_to_human(c2s_router_dir);
		if(dir == (int)'I')
		{
			return SESSION_DIRECTION_IN;
		}
		if(dir == (int)'E')
		{
			return SESSION_DIRECTION_OUT;
		}
	}
	return -1;
} 

int session_is_symmetric(struct session *sess, unsigned char *flag)
{
	int is_symmetric=0;
	if(sess->cur_pkt && sess->cur_pkt->a_stream)
	{
		if(sess->cur_pkt->a_stream->dir==DIR_DOUBLE && flag)
		{
			*flag=(SESSION_SEEN_C2S_FLOW|SESSION_SEEN_S2C_FLOW);
			is_symmetric=1;
		}
		else if(sess->cur_pkt->a_stream->dir==DIR_C2S && flag)
		{
			*flag=SESSION_SEEN_C2S_FLOW;
		}
		else if(sess->cur_pkt->a_stream->dir==DIR_S2C && flag)
		{
			*flag=SESSION_SEEN_S2C_FLOW;
		}
	}
	return is_symmetric;
}	

int session_is_outer_tunnel(struct session *sess, uint64_t *flag)
{
	if(sess==NULL||sess->cur_pkt==NULL||sess->cur_pkt->a_stream==NULL)
	{
		return 0;
	}
	struct streaminfo *t_stream = sess->cur_pkt->a_stream->pfather;
	struct streaminfo *a_stream = sess->cur_pkt->a_stream;
	if(flag)*flag=SESSION_IS_TUNNLE_NON;

	unsigned short tunnel_type=0;
	int tunnel_type_len=sizeof(unsigned short);
	MESA_get_stream_opt(a_stream, MSO_STREAM_UP_LAYER_TUNNEL_TYPE, (void *)&tunnel_type, &tunnel_type_len);
    if (tunnel_type != STREAM_TUNNLE_NON) // tunnel_type != STREAM_TUNNLE_NON means it is a outer of tunnel
    {
        if (flag)
            *flag = (uint64_t)tunnel_type;
        return 1;
    }

    while (t_stream != NULL)
	{
		if (t_stream->type == STREAM_TYPE_HTTP_PROXY)
		{
			if(flag) (*flag)|= (uint64_t)SESSION_IS_TUNNLE_HTTP_PROXY;
			return 1;
		}
		if(t_stream->type == STREAM_TYPE_SOCKS4 || t_stream->type == STREAM_TYPE_SOCKS5)
		{
			if(flag) (*flag)|= (uint64_t)SESSION_IS_TUNNLE_SOCKS;
			return 1;
		}
		t_stream = t_stream->pfather;
	}
	MESA_get_stream_opt(a_stream, MSO_STREAM_TUNNEL_TYPE, (void *)&tunnel_type, &tunnel_type_len);
	if(flag)*flag = (uint64_t)tunnel_type;
	return 0;
}

int session_is_inner_most(struct session *sess, uint64_t *flag)
{
	return !session_is_outer_tunnel(sess, flag);
}

int stellar_get_worker_thread_num(struct stellar *st)
{
	return get_thread_count();
}

int stellar_get_current_thread_id(struct stellar *st)
{
	return get_current_worker_thread_id();
}

int packet_arrive_time(const struct packet *pkt, struct timeval *ts)
{
	if(pkt == NULL || ts == NULL)return -1;
	if(pkt->raw_pkt == NULL || pkt->a_stream == NULL)return -1;

	get_opt_from_rawpkt(pkt->raw_pkt , RAW_PKT_GET_TIMESTAMP, &ts);
	if(ts->tv_sec == 0 && ts->tv_usec == 0)return -1;
	return 0;
}


const char *session_get0_current_l3_header(struct session *sess)
{
	if(sess == NULL || sess->cur_pkt == NULL || sess->cur_pkt->a_stream == NULL || sess->cur_pkt->raw_pkt == NULL) return NULL;
	return (const char *)get_current_layer3_header(sess->cur_pkt->a_stream);
}

const char *session_get0_current_l4_header(struct session *sess)
{
	if(sess == NULL || sess->cur_pkt == NULL || sess->cur_pkt->a_stream == NULL || sess->cur_pkt->raw_pkt == NULL) return NULL;
	return (const char *)get_current_layer4_header(sess->cur_pkt->a_stream);
}

int session_set_session_id(struct session *sess, uint64_t session_id)
{
	return MESA_set_stream_opt(sess->cur_pkt->a_stream, MSO_STREAM_SET_DATAMETA_TRACE_ID, (void *)&session_id, sizeof(uint64_t));
}

int session_set_preappend_segment_id_list(struct session *sess, uint16_t *sid, size_t sid_num)
{
	struct segment_id_list sid_list;
	sid_list.sz_sidlist=sid_num;
	memcpy(sid_list.sid_list, sid, sid_num*sizeof(uint16_t));
	return MESA_set_stream_opt(sess->cur_pkt->a_stream, MSO_STREAM_PREPLEND_SEGMENT_ID_LIST, (void *)&sid_list, sizeof(struct segment_id_list));

}

#if 0
const char *session_get0_l2_l3_hdr(struct session *sess, int packet_direction, size_t *l2_l3_hdr_len)
{
	int len = sizeof(int);
	const char *l2_header = NULL;
	*l2_l3_hdr_len = 0;
	if(sess == NULL || sess->cur_pkt == NULL || sess->cur_pkt->a_stream == NULL) return NULL;
	if(packet_direction!=PACKET_DIRECTION_C2S && packet_direction!=PACKET_DIRECTION_S2C) return NULL;
	enum MESA_stream_opt opt  = (packet_direction == PACKET_DIRECTION_C2S) ? MSO_STREAM_C2S_RAWPKT_HDR : MSO_STREAM_S2C_RAWPKT_HDR;
    if(MESA_get_stream_opt(sess->cur_pkt->a_stream, opt, (void *)&l2_header, &len) == -1) return NULL;
	*l2_l3_hdr_len = len;
	return l2_header;
}

#else
const char *session_get0_l2_l3_hdr(struct session *sess, int session_direction, size_t *l2_l3_hdr_len)
{
	int len = sizeof(int);
	const char *l2_header = NULL;
	*l2_l3_hdr_len = 0;
	if(sess == NULL || sess->cur_pkt == NULL || sess->cur_pkt->a_stream == NULL) return NULL;
	if(session_direction!=SESSION_DIRECTION_IN && session_direction!=SESSION_DIRECTION_OUT) return NULL;
	int c2s_sess_dir=session_get_direction(sess);
	enum MESA_stream_opt opt=(c2s_sess_dir==session_direction)?(MSO_STREAM_C2S_RAWPKT_HDR):(MSO_STREAM_S2C_RAWPKT_HDR);
    if(MESA_get_stream_opt(sess->cur_pkt->a_stream, opt, (void *)&l2_header, &len) == -1) return NULL;
	*l2_l3_hdr_len = len;
	return l2_header;
}
#endif


static const void *session_get_rawpkt(struct session *sess, int session_direction)
{
	if(sess == NULL || sess->cur_pkt == NULL || sess->cur_pkt->a_stream == NULL) return NULL;
	int route_direction=(session_direction==SESSION_DIRECTION_IN)?(DIR_ROUTE_UP):(DIR_ROUTE_DOWN);
	return get_rawpkt_from_streaminfo(sess->cur_pkt->a_stream, route_direction);
}

uint16_t *session_get0_segment_id_list(struct session *sess, int session_direction, size_t *sid_num)
{
	if(sess == NULL || sess->cur_pkt == NULL || sess->cur_pkt->a_stream == NULL) return NULL;
	*sid_num=0;
	const void *sapp_rawpkt=session_get_rawpkt(sess, session_direction);
	if(sapp_rawpkt==NULL)return NULL;
	struct segment_id_list *sids = NULL;
	int ret = get_rawpkt_opt_from_streaminfo(sess->cur_pkt->a_stream, RAW_PKT_GET_SID_LIST, &sids);
	if (ret != sizeof(struct segment_id_list))return NULL;
	*sid_num=sids->sz_sidlist;
	return sids->sid_list;
}
const char *session_get0_route_ctx(struct session *sess, int session_direction, size_t *route_ctx_len)
{
	if(sess == NULL || sess->cur_pkt == NULL || sess->cur_pkt->a_stream == NULL) return NULL;
	if(session_direction!=SESSION_DIRECTION_IN && session_direction!=SESSION_DIRECTION_OUT) return NULL;
	*route_ctx_len=0;
	const void *sapp_rawpkt=session_get_rawpkt(sess, session_direction);
	if(sapp_rawpkt==NULL)return NULL;
	void *route_ctx = NULL;
	int len=sizeof(route_ctx);
	enum MESA_stream_opt opt=((session_direction==SESSION_DIRECTION_IN)?(MSO_STREAM_INBOND_ROUTE_CTX):(MSO_STREAM_OUTBOND_ROUTE_CTX));
	int ret = MESA_get_stream_opt(sess->cur_pkt->a_stream, opt, &route_ctx, &len);
	if (ret < 0)return NULL;
	*route_ctx_len=len;
	return (const char *)route_ctx;
}

enum session_state session_get_current_state(struct session *sess)
{
	assert(sess);
    return sess->state;
}

int session_get_current_thread_id(struct session *sess)
{
	int thread_id = -1;
	if(sess)
	{
		if(sess->cur_pkt && sess->cur_pkt->a_stream)
		{
			thread_id = sess->cur_pkt->a_stream->threadnum;
		}
	}
	return thread_id;
}