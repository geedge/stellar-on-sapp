#include "stellar/stellar.h"
#include "stellar/utils.h"
#include "stellar/session_exdata.h"

#include "stellar_on_sapp.h"

#include <MESA/stream.h>

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

enum entry_type
{
	ENTRY_TYPE_UDP,
	ENTRY_TYPE_TCPALL,
	ENTRY_TYPE_TCP,
};

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
	session_free_on_sapp((struct session *)data);
	return;
}

int STELLAR_START_LOADER_INIT()
{
	g_stellar = stellar_init_on_sapp(STELLAR_PLUGIN_PATH);
	if(g_stellar==NULL)return -1;
	g_session_bridge_id=stream_bridge_build(STELLAR_BRIDEGE_NAME, "w");
	if(g_session_bridge_id < 0)return -1;
	stream_bridge_register_data_free_cb(g_session_bridge_id, stellar_on_sapp_bridge_free);
	g_streaminfo_exdata_id=stellar_session_exdata_new_index(g_stellar, STELLAR_EXDATA_NAME, NULL, NULL); 
	if(g_streaminfo_exdata_id < 0)return -1;
	return 0;
}

void STELLAR_START_LOADER_EXIT(void)
{
	stellar_exit_on_sapp(g_stellar);
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
	if(sess)
	{
		session_poll_on_sapp(sess);
		return APP_STATE_GIVEME;
	}
	else
	{
		return APP_STATE_DROPME;
	}
}

void  STELLAR_DEFER_LOADER_EXIT(void)
{
	return;
}



static unsigned char loader_transfer_stream_entry(struct streaminfo *pstream, UCHAR state, void **pme, int thread_seq,void *a_packet, enum entry_type type)
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

	//entry_type convert to packet_type
	enum packet_type pkt_type=UNKNOWN;
	switch (type)
	{
	case ENTRY_TYPE_UDP:
		pkt_type=UDP;
		break;
	case ENTRY_TYPE_TCP:
		pkt_type=TCP_STREAM;
		break;	
	case ENTRY_TYPE_TCPALL:
		pkt_type=TCP;
		break;	
	default:
		break;
	}

	switch (state)
	{
	case OP_STATE_PENDING:
        if (type == ENTRY_TYPE_TCPALL || type == ENTRY_TYPE_UDP)
        {
            ctx->sess = session_new_on_sapp(g_stellar, pstream);
            session_exdata_set(ctx->sess, g_streaminfo_exdata_id, pstream);
            stream_bridge_async_data_put(pstream, g_session_bridge_id, ctx->sess);
        }
        else
        {
            ctx->sess = (struct session *)stream_bridge_async_data_get(pstream, g_session_bridge_id);
        }
        entry_ret = session_state_update_on_sapp(pstream, ctx->sess, a_packet, pkt_type);
        break;
	case OP_STATE_DATA:
		raw_pkt = get_current_rawpkt_from_streaminfo(pstream);
		get_opt_from_rawpkt(raw_pkt, RAW_PKT_GET_IS_CTRL_PKT, (void *)&is_ctrl_pkt);
		if(is_ctrl_pkt==1)
		{
			entry_ret=session_state_update_on_sapp(pstream, ctx->sess, a_packet, CONTROL);
			entry_ret = entry_ret|APP_STATE_DROPPKT;
		}
		else
		{
			entry_ret=session_state_update_on_sapp(pstream, ctx->sess, a_packet, pkt_type);
		}
		break;
	case OP_STATE_CLOSE:
		// for TCPALL && UDP closed by idle timeout, definitely no packet
		// for TCP stream, only trigger update when packet payload > 0
		if(a_packet && pstream->ptcpdetail->datalen > 0)
		{
			entry_ret=session_state_update_on_sapp(pstream, ctx->sess, a_packet, pkt_type);
		}
		FREE(*pme);
		break;
	default:
		break;
	}
	return entry_ret;
}

unsigned char stellar_on_sapp_udp_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	return loader_transfer_stream_entry(pstream, pstream->opstate, pme,  thread_seq,  a_packet, ENTRY_TYPE_UDP);
}

unsigned char stellar_on_sapp_tcpall_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	return loader_transfer_stream_entry(pstream, pstream->pktstate, pme,  thread_seq,  a_packet, ENTRY_TYPE_TCPALL);
}

unsigned char stellar_on_sapp_tcp_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	
	return loader_transfer_stream_entry(pstream, pstream->opstate, pme,  thread_seq,  a_packet, ENTRY_TYPE_TCP);
}

char stellar_on_sapp_ip4_entry( struct streaminfo *pstream,unsigned char routedir,int thread_seq, void *a_packet)
{
	packet_update_on_sapp(g_stellar, pstream, a_packet, IPv4);
	return APP_STATE_GIVEME;
}

char stellar_on_sapp_ip6_entry( struct streaminfo *pstream,unsigned char routedir,int thread_seq, void *a_packet)
{
	packet_update_on_sapp(g_stellar, pstream, a_packet, IPv6);
	return APP_STATE_GIVEME;
}

char stellar_on_sapp_polling_entry(struct streaminfo *stream,  void **pme, int thread_seq,void *a_packet)
{
	polling_on_sapp(g_stellar);
	return APP_STATE_GIVEME;
}