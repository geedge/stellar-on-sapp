#include "session.h"
#include "utils.h"
#include "test_plugin.h"
#include "../adapter.h"
#include "../session_manager.h"

#include <MESA/stream.h>
#include <stdio.h>

struct plugin_spec g_plugin_schema[] =
{
	{
	.init_cb = test_plugin_init,
	.exit_cb = test_plugin_exit,
	.tcp_session_entry_cb=test_plugin_entry,
	.udp_session_entry_cb=test_plugin_entry,
	.tcp_session_interest_state=SESSION_STATE_ALL,
	.udp_session_interest_state=SESSION_STATE_ALL,
	},
};


struct adapter *g_adapter;
int TEST_INIT()
{
	g_adapter = adapter_init(g_plugin_schema, 1);
	if(g_adapter != NULL)return 1;
	return -1;
}

void TEST_EXIT(void)
{
	if(g_adapter)
	{
		adapter_exit(g_adapter);
		g_adapter = NULL;
	}
	return;
}

struct test_stream_ctx
{
	uint32_t c2s_pkts;
	uint32_t c2s_bytes;
	uint32_t  s2c_pkts;
	uint32_t  s2c_bytes;
	struct session *session;
};


static void print_steam_ctx(struct streaminfo *pstream, struct test_stream_ctx *ctx)
{
	printf("stream-----------%20s: ", printaddr(&(pstream->addr),	pstream->threadnum));
	printf("server-pkt=%u, server-count=%u, client-pkt=%u, client-count=%u, ",
	ctx->c2s_pkts, ctx->c2s_bytes,
	ctx->s2c_pkts, ctx->s2c_bytes);
	printf("total-pkt=%u, ", ctx->c2s_pkts+ctx->s2c_pkts);
	printf("total-count=%u\n", ctx->c2s_bytes+ctx->s2c_bytes);
	return;
}

static void loader_transfer_stream_entry(struct streaminfo *pstream, UCHAR state, void **pme, int thread_seq,void *a_packet)
{
	struct test_stream_ctx *ctx=*pme;
	struct tcpdetail *pdetail=(struct tcpdetail *)pstream->pdetail;
	if(*pme==NULL)
	{
		*pme=CALLOC(struct test_stream_ctx,1);
		ctx=*pme;
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
		adapter_session_open(g_adapter, pstream, &ctx->session, thread_seq);
		break;
	case OP_STATE_DATA:
		adapter_session_active(g_adapter, pstream, &ctx->session, thread_seq);
		break;
	case OP_STATE_CLOSE:
		adapter_session_close(g_adapter, pstream, &ctx->session, thread_seq);
		print_steam_ctx(pstream, ctx);
		FREE(*pme);
		break;
	default:
		break;
	}	
	return;
}

char test_udp_stream_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	loader_transfer_stream_entry(pstream, pstream->opstate, pme,  thread_seq,  a_packet);
	return APP_STATE_GIVEME;
}

char test_tcpall_stream_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	loader_transfer_stream_entry(pstream, pstream->pktstate, pme,  thread_seq,  a_packet);
	return APP_STATE_GIVEME;
}
