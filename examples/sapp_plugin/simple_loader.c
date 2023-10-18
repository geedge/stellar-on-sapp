#include "stellar/session.h"
#include "stellar/session_mq.h"
#include "stellar/utils.h"

#include "adapter/adapter.h"
#include "adapter/session_manager.h"

#include <MESA/stream.h>
#include <stdio.h>
#include <stream_inc/stream_base.h>


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


static void __loader_bridge_free_cb(const struct streaminfo *stream, int bridge_id, void *data)
{
	adapter_session_close((struct streaminfo *)stream, (struct session *)data, NULL);
	return;
}


struct stellar *g_stellar=NULL;
int g_session_bridge_id=-1;
int SIMPLE_LOADER_INIT()
{
	g_stellar = stellar_init(STELLAR_PLUGIN_PATH);
	if(g_stellar==NULL)return -1;
	g_session_bridge_id=stream_bridge_build(STELLAR_BRIDEGE_NAME, "w");
	if(g_session_bridge_id >= 0)// defer close session when streaminfo close
	{
		stream_bridge_register_data_free_cb(g_session_bridge_id, __loader_bridge_free_cb);
	}
	return 0;
}

void  SIMPLE_LOADER_EXIT(void)
{
	stellar_exit(g_stellar);
	return;
}

int DEFER_LOADER_INIT()
{
	return 0;
}

char defer_loader_stream_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	if(g_session_bridge_id >= 0)
	{
		struct session *sess = (struct session *)stream_bridge_async_data_get(pstream, g_session_bridge_id);
		adapter_session_poll(sess);
		return APP_STATE_GIVEME;
	}
	else
	{
		return APP_STATE_DROPME;
	}
}

void  DEFER_LOADER_EXIT(void)
{
	return;
}

static void loader_transfer_stream_entry(struct streaminfo *pstream, UCHAR state, void **pme, int thread_seq,void *a_packet)
{
	struct simple_stream_ctx *ctx=(struct simple_stream_ctx *)*pme;
	struct tcpdetail *pdetail=(struct tcpdetail *)pstream->pdetail;
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
		ctx->sess=adapter_session_open(g_stellar, pstream, a_packet);
		if(g_session_bridge_id>=0)
		{
			stream_bridge_async_data_put(pstream, g_session_bridge_id, ctx->sess);	
		}
		break;
	case OP_STATE_DATA:
		adapter_session_active(pstream, ctx->sess, a_packet);
		break;
	case OP_STATE_CLOSE:
        if (g_session_bridge_id < 0) // if no bridge, close session immediately
        {
            adapter_session_close(pstream, ctx->sess, a_packet);
        }
        FREE(*pme);
        break;
    default:
		break;
	}	
	return;
}

char simple_loader_udp_stream_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	loader_transfer_stream_entry(pstream, pstream->opstate, pme,  thread_seq,  a_packet);
	return APP_STATE_GIVEME;
}

char simple_loader_tcpall_stream_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	loader_transfer_stream_entry(pstream, pstream->pktstate, pme,  thread_seq,  a_packet);
	return APP_STATE_GIVEME;
}

char simple_loader_tcp_stream_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	loader_transfer_stream_entry(pstream, pstream->opstate, pme,  thread_seq,  a_packet);
	return APP_STATE_GIVEME;
}
