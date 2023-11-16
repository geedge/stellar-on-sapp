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

static void session_mq_topic_free(void *data, void *cb_arg)
{
	FREE(data);
	return;
}

static int session_mq_loader_read(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
	struct simple_stream_ctx *ctx =(struct simple_stream_ctx *)data;	
	printf("loader_read_message(topic:%d)-----------%20s", topic_id, session_get0_readable_addr(sess));
	printf("server-pkt=%u, server-count=%u, client-pkt=%u, client-count=%u, ",
	ctx->c2s_pkts, ctx->c2s_bytes,
	ctx->s2c_pkts, ctx->s2c_bytes);
	printf("total-pkt=%u, ", ctx->c2s_pkts+ctx->s2c_pkts);
	printf("total-count=%u\n", ctx->c2s_bytes+ctx->s2c_bytes);
	return 0;
}

struct stellar *g_stellar=NULL;
int g_topic_id=-1;
int PUBLISHER_LOADER_INIT()
{
	g_stellar = stellar_init("./stellar_plugin/simple_plugin.toml");
	if(g_stellar==NULL)return -1;
	int t_topic_id=session_mq_get_topic_id(g_stellar, "SIMPLE_MQ_TOPIC");
	if(t_topic_id >= 0)
	{
		session_mq_update_topic(g_stellar, t_topic_id, session_mq_topic_free, NULL);
		g_topic_id=t_topic_id;
	}
	else
	{
		g_topic_id=session_mq_create_topic(g_stellar, "SIMPLE_MQ_TOPIC", session_mq_topic_free, NULL);
	}
	session_mq_subscribe_topic(g_stellar , g_topic_id, session_mq_loader_read, NULL);
	return 0;
}

void PUBLISHER_LOADER_EXIT(void)
{

	session_mq_destroy_topic(g_stellar, g_topic_id);
	stellar_exit(g_stellar);
	return;
}


static void print_stream_ctx(struct streaminfo *pstream, struct simple_stream_ctx *ctx)
{
	printf("stream-----------%20s: ", printaddr(&(pstream->addr),	pstream->threadnum));
	printf("server-pkt=%u, server-count=%u, client-pkt=%u, client-count=%u, ",
	ctx->c2s_pkts, ctx->c2s_bytes,
	ctx->s2c_pkts, ctx->s2c_bytes);
	printf("total-pkt=%u, ", ctx->c2s_pkts+ctx->s2c_pkts);
	printf("total-count=%u\n", ctx->c2s_bytes+ctx->s2c_bytes);
	return;
}

static struct simple_stream_ctx *stream_ctx_dup(const struct simple_stream_ctx *origin)
{
	struct simple_stream_ctx *ctx=CALLOC(struct simple_stream_ctx,1);
	memcpy(ctx, origin, sizeof(struct simple_stream_ctx));
	return ctx;
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
	struct simple_stream_ctx *msg;
	switch (state)
	{
	case OP_STATE_PENDING:
		ctx->sess=adapter_session_new(g_stellar, pstream);
		adapter_session_state_update(pstream, ctx->sess, a_packet, SESSION_STATE_OPENING);
		break;
	case OP_STATE_DATA:
		msg = stream_ctx_dup((const struct simple_stream_ctx *)*pme);
		if(session_mq_publish_message(ctx->sess, g_topic_id, msg) < 0)
		{
			FREE(msg);
		}
		adapter_session_state_update(pstream, ctx->sess, a_packet, SESSION_STATE_ACTIVE);
		break;
	case OP_STATE_CLOSE:
		adapter_session_state_update(pstream, ctx->sess, a_packet, SESSION_STATE_CLOSING);
		adapter_session_free(ctx->sess);
		print_stream_ctx(pstream, ctx);
		FREE(*pme);
		break;
	default:
		break;
	}	
	return;
}

char publisher_loader_udp_stream_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	loader_transfer_stream_entry(pstream, pstream->opstate, pme,  thread_seq,  a_packet);
	return APP_STATE_GIVEME;
}

char publisher_loader_tcpall_stream_entry(struct streaminfo *pstream,void **pme, int thread_seq,void *a_packet)
{
	loader_transfer_stream_entry(pstream, pstream->pktstate, pme,  thread_seq,  a_packet);
	return APP_STATE_GIVEME;
}
