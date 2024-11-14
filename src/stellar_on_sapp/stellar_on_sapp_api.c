#include "stellar/session.h"
#include "stellar_on_sapp.h"

#include "stellar_internal.h"

#include "stellar/utils.h"

#include "stellar/stellar.h"
#include "stellar/session_mq.h"

#include "plugin_manager/plugin_manager.h"

#include <MESA/stream.h>
#include <string.h>
#include <assert.h>


#include <netinet/ip.h>
#include <netinet/ip6.h>

struct stellar
{
	struct plugin_manager_schema *plug_mgr;
	int tcp_stream_topic_id;
	struct packet *per_thread_pkt;
};

struct packet
{
	enum packet_type type;
	unsigned char ip_proto;
	const void *raw_pkt;
	struct streaminfo *pstream;
	struct session *sess;
	struct stellar *st;
}__attribute__((aligned(sizeof(void*)))) ;

struct session
{
	enum session_type type;
	enum session_state state;
	int session_direction;
	struct streaminfo *pstream;
	struct stellar *st;
	struct plugin_manager_runtime *plug_mgr_rt;
}__attribute__((aligned(sizeof(void*))));

inline struct stellar * packet_stellar_get(struct packet *pkt)
{
	return pkt->st;
}


inline struct plugin_manager_schema * stellar_plugin_manager_schema_get(struct stellar *st)
{
	return st->plug_mgr;
}

inline int stellar_plugin_manager_schema_set(struct stellar *st, struct plugin_manager_schema *pm)
{
	st->plug_mgr=pm;
	return 0;
}

inline struct plugin_manager_runtime * session_plugin_manager_runtime_get(struct session *sess)
{
	return sess->plug_mgr_rt;
}
/*********************************************
 * STELLAR INIT & EXIT ON SAPP *
 *********************************************/
struct stellar *stellar_init_on_sapp(const char *toml_conf_path)
{
	struct stellar *st = CALLOC(struct stellar, 1);
	struct plugin_manager_schema *pm=plugin_manager_init(st, toml_conf_path);
	if(pm==NULL)
	{
		FREE(st);
		return NULL;
	}
	st->plug_mgr=pm;
	st->tcp_stream_topic_id=stellar_session_mq_get_topic_id(st, TOPIC_TCP_STREAM);
	int tid=stellar_get_worker_thread_num(st);
	st->per_thread_pkt=CALLOC(struct packet, tid);
	for(int i=0; i<tid; i++)
	{
		st->per_thread_pkt[i].st=st;
	}
	return st;
}

void stellar_exit_on_sapp(struct stellar *st)
{
	if(!st)return;
	if(st->plug_mgr)plugin_manager_exit(st->plug_mgr);
	FREE(st->per_thread_pkt);
	FREE(st);
	return;	
};



/*********************************************
 * SESSION NEW & FREE ON SAPP *
 *********************************************/

struct session *session_new_on_sapp(struct stellar *st, struct streaminfo *stream)
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
	struct session *sess = CALLOC(struct session, 1);
	sess->st = st;
	sess->type = type;
	sess->state = SESSION_STATE_OPENING;
	sess->pstream=stream;
	sess->session_direction=-1;
	sess->plug_mgr_rt=plugin_manager_session_runtime_new(st->plug_mgr, sess);
    return sess;
}

void session_free_on_sapp(struct session *sess)
{
	sess->state = SESSION_STATE_CLOSING;
	plugin_manager_on_session_closing(sess);
	if(sess->plug_mgr_rt)
	{
		plugin_manager_session_runtime_free(sess->plug_mgr_rt);
	}
	FREE(sess);
	return;
}

/*********************************************
 * SESSION STATE UPDATE ON SAPP *
 *********************************************/

unsigned char session_state_update_on_sapp(struct streaminfo *stream, unsigned char stream_state, struct session *sess, const void *raw_pkt, enum packet_type type)
{
	if(sess)
	{
		sess->state=(stream_state == OP_STATE_PENDING) ? SESSION_STATE_OPENING : SESSION_STATE_ACTIVE;
		struct packet *pkt = stellar_get0_current_thread_packet(sess->st);
		pkt->raw_pkt=raw_pkt;
		pkt->type=type;
		pkt->sess=sess;
		pkt->st=sess->st;
		pkt->pstream=stream;
		if(raw_pkt)plugin_manager_on_session_ingress(sess, pkt);

		//check TCP topic active subscirber num, if 0, return APP_STATE_DROPME, to reduce tcp reassemble overhead		
		if(type == TCP_STREAM && session_mq_topic_is_active(sess, sess->st->tcp_stream_topic_id) == 0)
			return APP_STATE_DROPME;
		else
			return APP_STATE_GIVEME;
	}
	else
		return APP_STATE_DROPME;
}

void session_defer_on_sapp(struct session *sess)
{
	if(sess==NULL)return;
	struct packet *pkt = stellar_get0_current_thread_packet(sess->st);
	if(sess->state != SESSION_STATE_CONTROL)
	{
		if(unlikely(pkt->raw_pkt==NULL))pkt->raw_pkt=get_current_rawpkt_from_streaminfo(sess->pstream);
		if(pkt->raw_pkt)
		{
			plugin_manager_on_session_egress(sess, pkt);
			plugin_manager_on_packet_egress(sess->st->plug_mgr, pkt);
		}
	}
	pkt->raw_pkt=NULL;//clear raw_pkt
}

/*********************************************
 * PACKET STATE UPDATE ON SAPP *
 *********************************************/
#include <netinet/in.h>

struct packet *stellar_get0_current_thread_packet(struct stellar *st)
{
	int tid=get_current_worker_thread_id();
	return &st->per_thread_pkt[tid];
}

void packet_update_on_sapp(struct stellar *st, struct streaminfo *pstream, struct packet *pkt, void *pkt_hdr, enum packet_type type)
{
	pkt->type=type;
	pkt->raw_pkt=get_current_rawpkt_from_streaminfo(pstream);
	pkt->sess=NULL;
	pkt->pstream=pstream;
	switch (type)
	{
	case IPv4:
	{
		struct iphdr *ip4_hdr=(struct iphdr *)pkt_hdr;
		pkt->ip_proto=ip4_hdr->protocol;
		break;
	}
	case IPv6:
	{
		struct ip6_hdr *ip6_hdr=(struct ip6_hdr *)pkt_hdr;
		pkt->ip_proto=ip6_hdr->ip6_nxt;
		break;	
	}
	default:
		return;
	}
	//FIXME: defer TCP/UDP packet on session update
	plugin_manager_on_packet_ingress(st->plug_mgr, pkt);
	if(pkt->ip_proto!=IPPROTO_TCP && pkt->ip_proto!=IPPROTO_UDP)
	{
		plugin_manager_on_packet_egress(st->plug_mgr, pkt);
	}

}

inline int polling_on_sapp(struct stellar *st)
{
	return plugin_manager_on_polling(st->plug_mgr);
}

/*********************************************
 * STELLAR INFO INTERFACE WRAPPER ON SAPP
 *********************************************/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
int stellar_get_worker_thread_num(struct stellar *st)
{
	return get_thread_count();
}

int stellar_get_current_thread_id(struct stellar *st)
{
	return get_current_worker_thread_id();
}
#pragma GCC diagnostic pop

/*********************************************
 * PACKET INFO INTERFACE WRAPPER ON SAPP*
 *********************************************/


inline enum packet_type packet_get_type(const struct packet *pkt)
{
	return pkt->type;
}

int packet_get_direction(const struct packet *pkt)
{
	struct session *sess = pkt->sess;
	if(sess==NULL)return PACKET_DIRECTION_UNKNOWN;

	const struct streaminfo *pstream=sess->pstream;
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

int packet_arrive_time(const struct packet *pkt, struct timeval *ts)
{
	if(pkt == NULL || ts == NULL)return -1;
	if(pkt->raw_pkt == NULL)return -1;

	get_opt_from_rawpkt(pkt->raw_pkt , RAW_PKT_GET_TIMESTAMP, ts);
	if(ts->tv_sec == 0 && ts->tv_usec == 0)return -1;
	return 0;
}

inline unsigned char packet_get_ip_protocol(struct packet *pkt)
{
	if(pkt == NULL)return 0;
	return pkt->ip_proto;
}

int packet_get_route_direction(const struct packet *pkt)
{
	if(pkt == NULL || pkt->raw_pkt==NULL)return PACKET_DIRECTION_UNKNOWN;
	int dir=PACKET_DIRECTION_UNKNOWN;
	get_opt_from_rawpkt(pkt->raw_pkt, RAW_PKT_GET_ROUTE_DIR, &dir);
	dir=MESA_dir_link_to_human(dir);
	if (dir == (int)'I' || dir == (int)'i')
    {
        return PACKET_DIRECTION_INCOMING;
    }
    if (dir == (int)'E' || dir == (int)'e')
    {
        return PACKET_DIRECTION_OUTGOING;
    }
	return PACKET_DIRECTION_UNKNOWN;
}

int packet_get_linkid(const struct packet *pkt)
{
	if(pkt == NULL || pkt->raw_pkt==NULL)return -1;
	int link_id = -1;
	get_opt_from_rawpkt(pkt->raw_pkt, RAW_PKT_GET_LINK_ID, &link_id);
	return link_id;
}

void packet_drop(const struct packet *pkt)
{
	if(pkt == NULL || pkt->pstream==NULL)return;
	int value=1;
	MESA_set_stream_opt(pkt->pstream, MSO_DROP_CURRENT_PKT, &value, sizeof(value));
}

const char *packet_get0_readable_addr(struct packet *pkt)
{
	if(pkt == NULL || pkt->pstream==NULL)return NULL;
	struct streaminfo *pstream=pkt->pstream;
	return printaddr(&pstream->addr, pstream->threadnum);
}

int packet_get_current_thread_id(struct packet *pkt)
{
	int thread_id = -1;
	struct streaminfo *pstream=pkt->pstream;
	assert(pstream);
	thread_id = pstream->threadnum;
	return thread_id;
}

inline struct session *packet_get_session(const struct packet *pkt)
{
	if(pkt == NULL)return NULL;
	return pkt->sess;
}

/*********************************************
 * SESSION INFO INTERFACE WRAPPER ON SAPP*
 *********************************************/

inline struct packet *session_get_current_packet(struct session *sess)
{
	return stellar_get0_current_thread_packet(sess->st);
}

const char* session_get0_readable_addr(struct session *sess)
{
	struct streaminfo *pstream=sess->pstream;
	assert(pstream);
	return printaddr(&pstream->addr, pstream->threadnum);
}

struct session_addr *session_get0_addr(struct session *sess, enum session_addr_type *addr_type)
{
    struct session_addr *addr = NULL;
    *addr_type = SESSION_ADDR_TYPE_UNKNOWN;
    struct streaminfo *pstream = sess->pstream;
    assert(pstream);
    if (pstream->addr.addrtype == ADDR_TYPE_IPV4)
    {
        if (pstream->type == STREAM_TYPE_TCP)
        {
            *addr_type = SESSION_ADDR_TYPE_IPV4_TCP;
        }
        else if (pstream->type == STREAM_TYPE_UDP)
        {
            *addr_type = SESSION_ADDR_TYPE_IPV4_UDP;
        }
        addr = (struct session_addr *)pstream->addr.tuple4_v4;
    }
    if (pstream->addr.addrtype == ADDR_TYPE_IPV6)
    {
        if (pstream->type == STREAM_TYPE_TCP)
        {
            *addr_type = SESSION_ADDR_TYPE_IPV6_TCP;
        }
        else if (pstream->type == STREAM_TYPE_UDP)
        {
            *addr_type = SESSION_ADDR_TYPE_IPV6_UDP;
        }
        addr = (struct session_addr *)pstream->addr.tuple4_v6;
    }
    return addr;
}

const char *session_get0_current_payload(struct session *sess, size_t *payload_len)
{
    struct streaminfo *pstream = sess->pstream;
    assert(pstream);
	*payload_len = (size_t)pstream->ptcpdetail->datalen;
	return (const char*)pstream->ptcpdetail->pdata;
}

inline const struct packet *session_get0_current_packet(struct session *sess)
{
	struct packet *pkt=stellar_get0_current_thread_packet(sess->st);	
	if(unlikely(pkt->raw_pkt == NULL))
	{
		struct streaminfo *pstream=sess->pstream;
		assert(pstream);
		pkt->pstream=pstream;
		pkt->sess=sess;
		//attach packet to session
		pkt->raw_pkt=(void *)get_current_rawpkt_from_streaminfo(pstream);
	}
	return pkt;
}

int  session_get_direction(struct session *sess)
{
	if(sess->session_direction>=0)return sess->session_direction;
	struct streaminfo *pstream=sess->pstream;
	assert(pstream);
    u_int8_t c2s_router_dir = 0;
    int dir_len = sizeof(c2s_router_dir);
    MESA_get_stream_opt(pstream, MSO_STREAM_C2S_ROUTE_DIRECTION, (void *)&c2s_router_dir, &dir_len);
    int dir = MESA_dir_link_to_human(c2s_router_dir);
    if (dir == (int)'I')
    {
        sess->session_direction = SESSION_DIRECTION_IN;
    }
    if (dir == (int)'E')
    {
        sess->session_direction = SESSION_DIRECTION_OUT;
    }
    return sess->session_direction;

} 

int session_is_symmetric(struct session *sess, unsigned char *flag)
{
	int is_symmetric=0;
	struct streaminfo *pstream=sess->pstream;
	assert(pstream);
    if (pstream->dir == DIR_DOUBLE)
    {
		if(flag)*flag = (SESSION_SEEN_C2S_FLOW | SESSION_SEEN_S2C_FLOW);
        is_symmetric = 1;
    }
    else if (pstream->dir == DIR_C2S)
    {
		if(flag)*flag = SESSION_SEEN_C2S_FLOW;
		
    }
    else if (pstream->dir == DIR_S2C)
    {
		if(flag)*flag = SESSION_SEEN_S2C_FLOW;
    }
    return is_symmetric;

}	

int session_is_outmost(struct session *sess, uint64_t *flag)
{
	struct streaminfo *a_stream=sess->pstream;
	assert(a_stream);
	struct streaminfo *t_stream = a_stream->pfather;
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

int session_is_innermost(struct session *sess, uint64_t *flag)
{
	return !session_is_outmost(sess, flag);
}

const char *session_get0_current_l3_header(struct session *sess)
{
	struct streaminfo *pstream=sess->pstream;
	assert(pstream);
	return (const char *)get_current_layer3_header(pstream);
}

const char *session_get0_current_l4_header(struct session *sess)
{
	struct streaminfo *pstream=sess->pstream;
	assert(pstream);
	return (const char *)get_current_layer4_header(pstream);
}

int session_set_session_id(struct session *sess, uint64_t session_id)
{
	struct streaminfo *pstream=sess->pstream;
	assert(pstream);
	return MESA_set_stream_opt(pstream, MSO_STREAM_SET_DATAMETA_TRACE_ID, (void *)&session_id, sizeof(uint64_t));
}

int session_set_preappend_segment_id_list(struct session *sess, uint16_t *sid, size_t sid_num)
{
	struct streaminfo *pstream=sess->pstream;
	assert(pstream);
	struct segment_id_list sid_list;
	sid_list.sz_sidlist=sid_num;
	memcpy(sid_list.sid_list, sid, sid_num*sizeof(uint16_t));
	return MESA_set_stream_opt(pstream, MSO_STREAM_PREPLEND_SEGMENT_ID_LIST, (void *)&sid_list, sizeof(struct segment_id_list));

}

const char *session_get0_l2_l3_hdr(struct session *sess, int session_direction, size_t *l2_l3_hdr_len)
{
	int len = sizeof(int);
	const char *l2_header = NULL;
	*l2_l3_hdr_len = 0;
	if(session_direction!=SESSION_DIRECTION_IN && session_direction!=SESSION_DIRECTION_OUT) return NULL;

	struct streaminfo *pstream=sess->pstream;
	assert(pstream);

	int c2s_sess_dir=session_get_direction(sess);
	
	enum MESA_stream_opt opt=(c2s_sess_dir==session_direction)?(MSO_STREAM_C2S_RAWPKT_HDR):(MSO_STREAM_S2C_RAWPKT_HDR);
    if(MESA_get_stream_opt(pstream, opt, (void *)&l2_header, &len) == -1) return NULL;
	*l2_l3_hdr_len = len;
	return l2_header;
}


uint16_t *session_get0_segment_id_list(struct session *sess, int session_direction, size_t *sid_num)
{
	*sid_num=0;
	struct streaminfo *pstream=sess->pstream;
	assert(pstream);
	struct segment_id_list *sids = NULL;
	int len=sizeof(void*);
	enum MESA_stream_opt opt=((session_direction==SESSION_DIRECTION_IN)?(MSO_STREAM_INBOND_SEGMENT_ID_LIST):(MSO_STREAM_OUTBOND_SEGMENT_ID_LIST));
	int ret = MESA_get_stream_opt(pstream, opt, &sids, &len);
	if (ret != sizeof(struct segment_id_list))return NULL;
	*sid_num=sids->sz_sidlist;
	return sids->sid_list;
}

const char *session_get0_route_ctx(struct session *sess, int session_direction, size_t *route_ctx_len)
{
	struct streaminfo *pstream=sess->pstream;
	assert(pstream);
	if(session_direction!=SESSION_DIRECTION_IN && session_direction!=SESSION_DIRECTION_OUT) return NULL;
	*route_ctx_len=0;
	void *route_ctx = NULL;
	int len=sizeof(route_ctx);
	enum MESA_stream_opt opt=((session_direction==SESSION_DIRECTION_IN)?(MSO_STREAM_INBOND_ROUTE_CTX):(MSO_STREAM_OUTBOND_ROUTE_CTX));
	int ret = MESA_get_stream_opt(pstream, opt, &route_ctx, &len);
	if (ret < 0)return NULL;
	*route_ctx_len=len;
	return (const char *)route_ctx;
}

inline enum session_state session_get_current_state(struct session *sess)
{
    return sess->state;
}

inline void session_set_current_state(struct session *sess, enum session_state state)
{
    sess->state = state;
}

inline enum session_type session_get_type(struct session *sess)
{
    return sess->type;
}

int session_get_current_thread_id(struct session *sess)
{
	int thread_id = -1;
	struct streaminfo *pstream=sess->pstream;
	assert(pstream);
	thread_id = pstream->threadnum;
	return thread_id;
}

long long session_get_client_isn(struct session *sess)
{
	struct streaminfo *pstream=sess->pstream;
	assert(pstream);
    uint32_t client_isn=0;
    int client_isn_sz=sizeof(client_isn);
    if(MESA_get_stream_opt(pstream, MSO_TCP_ISN_C2S, &(client_isn), &client_isn_sz)!=0)return -1;
    return client_isn;
}

long long session_get_server_isn(struct session *sess)
{
	struct streaminfo *pstream=sess->pstream;
	assert(pstream);
    uint32_t server_isn=0;
    int server_isn_sz=sizeof(server_isn);
    if(MESA_get_stream_opt(pstream, MSO_TCP_ISN_S2C, &(server_isn), &server_isn_sz)!=0)return -1;
    return server_isn;
}

void stellar_emit_datapath_telemetry(struct packet *pkt,  const char * module, const char *str)
{
	if(pkt==NULL)return;
	if(pkt->raw_pkt==NULL)return;
	sapp_emit_datapath_telemetry(pkt->raw_pkt, module, str);
}