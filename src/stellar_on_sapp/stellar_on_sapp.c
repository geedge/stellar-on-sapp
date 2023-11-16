#include "stellar/utils.h"
#include "stellar/session_exdata.h"

#include "adapter/adapter.h"

#include <MESA/stream.h>
#include <stream_inc/stream_base.h>

#include <assert.h>

struct simple_stream_ctx
{
	uint32_t c2s_pkts;
	uint32_t c2s_bytes;
	uint32_t  s2c_pkts;
	uint32_t  s2c_bytes;
	struct session *sess;
};

#define STELLAR_PLUGIN_PATH "./stellar_plugin/spec.toml"
#define STELLAR_BRIDEGE_NAME "STELLAR_SESSION"
#define STELLAR_EXDATA_NAME "SAPP_STREAMINFO"


struct stellar *g_stellar=NULL;
int g_session_bridge_id=-1;
int g_streaminfo_exdata_id=-1;

static int is_l7_type_tunnels(struct streaminfo *a_stream)
{
	const struct streaminfo *pfather=NULL, *ptmp=a_stream;
	while(ptmp)
	{
		pfather = ptmp->pfather;
		switch(ptmp->type)
		{
			case	STREAM_TYPE_SOCKS4:
			case	STREAM_TYPE_SOCKS5:
			case	STREAM_TYPE_HTTP_PROXY:
				return 1;
			default:
				break;
		}
		ptmp = pfather;
	}
	return 0;
}

static void stellar_on_sapp_bridge_free(const struct streaminfo *stream, int bridge_id, void *data)
{
	adapter_session_state_update((struct streaminfo *)stream, (struct session *)data, NULL, SESSION_STATE_CLOSING);	
	adapter_session_free((struct session *)data);
	return;
}

int STELLAR_START_LOADER_INIT()
{
	g_stellar = stellar_init(STELLAR_PLUGIN_PATH);
	if(g_stellar==NULL)return -1;
	g_session_bridge_id=stream_bridge_build(STELLAR_BRIDEGE_NAME, "w");
	if(g_session_bridge_id < 0)return -1;
	stream_bridge_register_data_free_cb(g_session_bridge_id, stellar_on_sapp_bridge_free);
	g_streaminfo_exdata_id=stellar_session_get_ex_new_index(g_stellar, STELLAR_EXDATA_NAME, NULL, NULL); 
	if(g_streaminfo_exdata_id < 0)return -1;
	return 0;
}

void STELLAR_START_LOADER_EXIT(void)
{
	stellar_exit(g_stellar);
	return;
}

int STELLAR_DEFER_LOADER_INIT()
{
	return 0;
}

char stellar_on_sapp_defer_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	if(pstream->pktstate==OP_STATE_PENDING && is_l7_type_tunnels(pstream)==1)
	{
		return APP_STATE_DROPME;
	}
	struct session *sess = (struct session *)stream_bridge_async_data_get(pstream, g_session_bridge_id);
	assert(sess);
	adapter_session_poll(sess);
	return APP_STATE_GIVEME;
}

void  STELLAR_DEFER_LOADER_EXIT(void)
{
	return;
}

static unsigned char loader_transfer_stream_entry(struct streaminfo *pstream, UCHAR state, void **pme, int thread_seq,void *a_packet)
{
	struct simple_stream_ctx *ctx=(struct simple_stream_ctx *)*pme;
	struct tcpdetail *pdetail=(struct tcpdetail *)pstream->pdetail;
	int is_ctrl_pkt=0;
	const void *raw_pkt=NULL;
	unsigned char entry_ret=APP_STATE_GIVEME;
	if(state == OP_STATE_PENDING && (is_l7_type_tunnels(pstream)==1)) 
	{
		return APP_STATE_DROPME;
	}

	if(*pme==NULL)
	{
		*pme=CALLOC(struct simple_stream_ctx,1);
		ctx=(struct simple_stream_ctx *)*pme;
	}

	if(a_packet!=NULL)
	{
		if(DIR_C2S == pstream->curdir){
			ctx->c2s_bytes += pdetail->datalen;
			ctx->c2s_pkts++;
		}else{
			ctx->s2c_bytes += pdetail->datalen;
			ctx->s2c_pkts++;
		}  		
	}
	switch (state)
	{
	case OP_STATE_PENDING:
		ctx->sess=adapter_session_new(g_stellar, pstream);
		session_set_ex_data(ctx->sess, g_streaminfo_exdata_id, pstream);
		stream_bridge_async_data_put(pstream, g_session_bridge_id, ctx->sess);
		entry_ret=adapter_session_state_update(pstream, ctx->sess, a_packet, SESSION_STATE_OPENING);
		break;
	case OP_STATE_DATA:
		raw_pkt = get_current_rawpkt_from_streaminfo(pstream);
		get_opt_from_rawpkt(raw_pkt, RAW_PKT_GET_IS_CTRL_PKT, (void *)&is_ctrl_pkt);
		if(is_ctrl_pkt==1)
		{
			entry_ret=adapter_session_state_update(pstream, ctx->sess, a_packet, SESSION_STATE_CONTROL);
			entry_ret = entry_ret|APP_STATE_DROPPKT;
		}
		else
		{
			entry_ret=adapter_session_state_update(pstream, ctx->sess, a_packet, SESSION_STATE_ACTIVE);
		}
		break;
	case OP_STATE_CLOSE:
		FREE(*pme);
		break;
	default:
		break;
	}
	return entry_ret;
}

unsigned char stellar_on_sapp_udp_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	return loader_transfer_stream_entry(pstream, pstream->opstate, pme,  thread_seq,  a_packet);
}

unsigned char stellar_on_sapp_tcpall_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	return loader_transfer_stream_entry(pstream, pstream->pktstate, pme,  thread_seq,  a_packet);
}

unsigned char stellar_on_sapp_tcp_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	return loader_transfer_stream_entry(pstream, pstream->opstate, pme,  thread_seq,  a_packet);
}
