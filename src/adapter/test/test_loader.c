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
	.init_cb = test_entry_plugin_init,
	.exit_cb = test_entry_plugin_exit,
	.tcp_session_entry_cb=test_entry_plugin_entry,
	.udp_session_entry_cb=test_entry_plugin_entry,
	.tcp_session_interest_state=SESSION_STATE_ALL,
	.udp_session_interest_state=SESSION_STATE_ALL,
	},
	{
	.init_cb = test_mq_plugin_init,
	.exit_cb = test_mq_plugin_exit,
	.tcp_session_entry_cb=test_mq_plugin_entry,
	.udp_session_entry_cb=test_mq_plugin_entry,
	.tcp_session_interest_state=SESSION_STATE_ALL,
	.udp_session_interest_state=SESSION_STATE_ALL,
	},
};

struct test_stream_ctx
{
	uint32_t c2s_pkts;
	uint32_t c2s_bytes;
	uint32_t  s2c_pkts;
	uint32_t  s2c_bytes;
	struct session *session;
};

static void test_mq_topic_free(struct session *sess, const char* topic_name, void *data, void *cb_arg)
{
	FREE(data);
	return;
}

int  test_mq_loader_read(struct session *sess, const char* topic_name, void *data, void *cb_arg)
{
	struct test_stream_ctx *ctx =(struct test_stream_ctx *)data;	
	printf("loader_read_message(%s)-----------%20s", topic_name, session_printaddr(sess));
	printf("server-pkt=%u, server-count=%u, client-pkt=%u, client-count=%u, ",
	ctx->c2s_pkts, ctx->c2s_bytes,
	ctx->s2c_pkts, ctx->s2c_bytes);
	printf("total-pkt=%u, ", ctx->c2s_pkts+ctx->s2c_pkts);
	printf("total-count=%u\n", ctx->c2s_bytes+ctx->s2c_bytes);
	return 0;
}

struct adapter *g_adapter;
int TEST_INIT()
{
	g_adapter = adapter_init(g_plugin_schema, 2);
	if(g_adapter == NULL)return -1;

	session_mq_topic_create("test_mq_topic", test_mq_topic_free, NULL);
	session_mq_topic_subscribe("test_mq_topic", test_mq_loader_read, NULL);
	return 0;
}

void TEST_EXIT(void)
{
	if(g_adapter)
	{
		session_mq_topic_unsubscribe("test_mq_topic", test_mq_loader_read, NULL);
		session_mq_topic_destroy("test_mq_topic");
		adapter_exit(g_adapter);
		g_adapter = NULL;
	}
	return;
}


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

static struct test_stream_ctx *stream_ctx_dup(struct test_stream_ctx *origin)
{
	struct test_stream_ctx *ctx=CALLOC(struct test_stream_ctx,1);
	memcpy(ctx, origin, sizeof(struct test_stream_ctx));
	return ctx;
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
	struct test_stream_ctx *msg;
	switch (state)
	{
	case OP_STATE_PENDING:
		adapter_session_open(g_adapter, pstream, &ctx->session, thread_seq);
		break;
	case OP_STATE_DATA:
		msg = stream_ctx_dup(*pme);
		if(session_mq_send_msg(ctx->session, "test_mq_topic", msg) < 0)
		{
			FREE(msg);
		}
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
