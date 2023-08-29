#include "stellar/session.h"
#include "stellar/session_mq.h"
#include "stellar/utils.h"

#include "adapter/adapter.h"
#include "adapter/session_manager.h"

#include <MESA/stream.h>
#include <stdio.h>


struct simple_stream_ctx
{
	uint32_t c2s_pkts;
	uint32_t c2s_bytes;
	uint32_t  s2c_pkts;
	uint32_t  s2c_bytes;
	struct session *sess;
};

#define STELLAR_PLUGIN_PATH "./stellar_plugin/spec.toml"

void *g_stellar=NULL;
int g_topic_id=-1;
int SIMPLE_LOADER_INIT()
{
	g_stellar = stellar_init(STELLAR_PLUGIN_PATH);
	if(g_stellar==NULL)return -1;
	return 0;
}

void  SIMPLE_LOADER_EXIT(void)
{

	stellar_exit(g_stellar);
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
		break;
	case OP_STATE_DATA:
		adapter_session_active(pstream, ctx->sess, a_packet);
		break;
	case OP_STATE_CLOSE:
		adapter_session_close(pstream, ctx->sess, a_packet);
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
