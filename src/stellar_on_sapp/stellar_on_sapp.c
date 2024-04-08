#include "stellar/session_exdata.h"

#include "adapter/adapter.h"

#include <MESA/stream.h>
#include <stream_inc/stream_base.h>

#define STELLAR_PLUGIN_PATH "./stellar_plugin/spec.toml"
#define STELLAR_BRIDEGE_NAME "STELLAR_SESSION"
#define STELLAR_EXDATA_NAME "SAPP_STREAMINFO"

#include <assert.h>

struct stellar *g_stellar=NULL;
int g_session_bridge_id=-1;
int g_streaminfo_exdata_id=-1;

static int stream_is_inner_most(struct streaminfo *a_stream)
{
	unsigned short tunnel_type=0;
	int tunnel_type_len=sizeof(unsigned short);
	MESA_get_stream_opt(a_stream, MSO_STREAM_UP_LAYER_TUNNEL_TYPE, (void *)&tunnel_type, &tunnel_type_len);
    if (tunnel_type&STREAM_TUNNEL_GPRS_TUNNEL) // tunnel_type set STREAM_TUNNEL_GPRS_TUNNEL means it is a outer of GTP tunnel
    {
        return 0;
    }
	return 1;
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
	assert(g_stellar!=NULL);
	g_session_bridge_id=stream_bridge_build(STELLAR_BRIDEGE_NAME, "w");
	assert(g_session_bridge_id >= 0);
	stream_bridge_register_data_free_cb(g_session_bridge_id, stellar_on_sapp_bridge_free);
	g_streaminfo_exdata_id=stellar_session_get_ex_new_index(g_stellar, STELLAR_EXDATA_NAME, NULL, NULL); 
	assert(g_streaminfo_exdata_id >= 0);
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
	if(pstream->pktstate==OP_STATE_PENDING && stream_is_inner_most(pstream)==0)
	{
		return APP_STATE_DROPME;
	}
	struct session *sess = (struct session *)stream_bridge_async_data_get(pstream, g_session_bridge_id);
	assert(sess!=NULL);
	adapter_session_poll(sess);
	return APP_STATE_GIVEME;
}

void  STELLAR_DEFER_LOADER_EXIT(void)
{
	return;
}

static unsigned char loader_transfer_stream_entry(struct streaminfo *pstream, UCHAR state, void **pme, int thread_seq,void *a_packet)
{
	struct session *sess=(struct session *)*pme;
	int is_ctrl_pkt=0;
	const void *raw_pkt=NULL;
	unsigned char entry_ret=APP_STATE_GIVEME;
	if(state == OP_STATE_PENDING && (stream_is_inner_most(pstream)==0)) 
	{
		return APP_STATE_DROPME;
	}

	switch (state)
	{
	case OP_STATE_PENDING:
		sess=adapter_session_new(g_stellar, pstream);
		session_set_ex_data(sess, g_streaminfo_exdata_id, pstream);
		stream_bridge_async_data_put(pstream, g_session_bridge_id, sess);
		entry_ret=adapter_session_state_update(pstream, sess, a_packet, SESSION_STATE_OPENING);
		*pme=sess;
		break;
	case OP_STATE_DATA:
		raw_pkt = get_current_rawpkt_from_streaminfo(pstream);
		get_opt_from_rawpkt(raw_pkt, RAW_PKT_GET_IS_CTRL_PKT, (void *)&is_ctrl_pkt);
		if(is_ctrl_pkt==1)
		{
			entry_ret=adapter_session_state_update(pstream, sess, a_packet, SESSION_STATE_CONTROL);
			entry_ret = entry_ret|APP_STATE_DROPPKT;
		}
		else
		{
			entry_ret=adapter_session_state_update(pstream, sess, a_packet, SESSION_STATE_ACTIVE);
		}
		break;
	case OP_STATE_CLOSE:
		if(a_packet)
		{
			entry_ret=adapter_session_state_update(pstream, sess, a_packet, SESSION_STATE_ACTIVE);
		}
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

char stellar_on_sapp_ip4_entry( struct streaminfo *pstream,unsigned char routedir,int thread_seq, void *a_packet)
{
	adapter_on_packet_update(g_stellar, pstream, a_packet, IPv4);
	return APP_STATE_GIVEME;
}

char stellar_on_sapp_ip6_entry( struct streaminfo *pstream,unsigned char routedir,int thread_seq, void *a_packet)
{
	adapter_on_packet_update(g_stellar, pstream, a_packet, IPv6);
	return APP_STATE_GIVEME;
}

char stellar_on_sapp_polling_entry(struct streaminfo *stream,  void **pme, int thread_seq,void *a_packet)
{
	int is_working=adapter_on_polling(g_stellar);
	if(is_working==1)return POLLING_STATE_WORK;
	return POLLING_STATE_IDLE;
}
