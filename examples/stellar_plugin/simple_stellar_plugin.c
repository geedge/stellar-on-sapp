#include "stellar/stellar.h"
#include "stellar/utils.h"
#include "stellar/session_exdata.h"
#include "stellar/session_mq.h"

#include <stdio.h>

struct simple_stellar_plugin_ctx
{
    int plugin_id;
    int exdata_idx;
	struct stellar *st;
};

struct mq_message_stat
{
	uint32_t c2s_pkts;
	uint32_t c2s_bytes;
	uint32_t s2c_pkts;
	uint32_t s2c_bytes;
};

extern int simple_mq_plugin_entry(struct session *sess, int events, const struct packet *pkt, void *cb_arg);

static int  session_mq_plugin_sub_fn(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
	struct mq_message_stat *ctx =(struct mq_message_stat *)data;	
	struct simple_stellar_plugin_ctx *plugin_ctx = (struct simple_stellar_plugin_ctx *)cb_arg;
	printf("%s(topic:%d->plug:%d)-----------%20s: ", __FUNCTION__, topic_id, plugin_ctx->plugin_id, session_get0_readable_addr(sess));
	printf("server-pkt=%u, server-count=%u, client-pkt=%u, client-count=%u, ",
	ctx->c2s_pkts, ctx->c2s_bytes,
	ctx->s2c_pkts, ctx->s2c_bytes);
	printf("total-pkt=%u, ", ctx->c2s_pkts+ctx->s2c_pkts);
	printf("total-count=%u\n", ctx->c2s_bytes+ctx->s2c_bytes);

	struct session_event *i_ev=session_get_intrinsic_event(sess, plugin_ctx->plugin_id);
	session_event_assign(i_ev, plugin_ctx->st, sess, (SESS_EV_TCP|SESS_EV_UDP|SESS_EV_OPENING|SESS_EV_PACKET|SESS_EV_CLOSING), simple_mq_plugin_entry, plugin_ctx);
	printf("%s(plug:%d)session_event_assign-----------%20s\n", __FUNCTION__, plugin_ctx->plugin_id, session_get0_readable_addr(sess));

	return 0;
}

static void print_session_ctx(struct session *sess, struct mq_message_stat *ctx, int plugin_id)
{
	printf("%s(plug:%d)-----------%20s: ", __FUNCTION__, plugin_id, session_get0_readable_addr(sess));
	printf("server-pkt=%u, server-count=%u, client-pkt=%u, client-count=%u, ",
	ctx->c2s_pkts, ctx->c2s_bytes,
	ctx->s2c_pkts, ctx->s2c_bytes);
	printf("total-pkt=%u, ", ctx->c2s_pkts+ctx->s2c_pkts);
	printf("total-count=%u\n", ctx->c2s_bytes+ctx->s2c_bytes);
	return;
}

int simple_event_plugin_entry(struct session *sess, int events, const struct packet *pkt, void *cb_arg)
{
	if(cb_arg == NULL)return -1;
	struct simple_stellar_plugin_ctx *plugin_ctx=(struct simple_stellar_plugin_ctx *)cb_arg;
	struct mq_message_stat *mg_stat = (struct mq_message_stat *)session_get_ex_data(sess, plugin_ctx->exdata_idx);
	if (mg_stat == NULL)
	{
		mg_stat = CALLOC(struct mq_message_stat, 1);
		session_set_ex_data(sess, plugin_ctx->exdata_idx, mg_stat);
	}

	if (pkt)
	{
        size_t payload_len = 0;
        session_get0_current_payload(sess, &payload_len);
        int dir = session_get_direction(sess);
		if (dir==SESSION_DIRECTION_IN)
		{
			mg_stat->c2s_bytes += payload_len;
			mg_stat->c2s_pkts += 1;
		}
		if (dir==SESSION_DIRECTION_OUT)
		{
			mg_stat->s2c_bytes += payload_len;
			mg_stat->s2c_pkts += 1;
		}
	}
    if (mg_stat != NULL && (events & SESS_EV_CLOSING))
    {
        print_session_ctx(sess, mg_stat, plugin_ctx->plugin_id);
    }
    return 0;
}

int simple_mq_plugin_entry(struct session *sess, int events, const struct packet *pkt, void *cb_arg)
{
	if(cb_arg == NULL)return -1;
	struct simple_stellar_plugin_ctx *plugin_ctx=(struct simple_stellar_plugin_ctx *)cb_arg;
	struct session_event *i_ev=session_get_intrinsic_event(sess, plugin_ctx->plugin_id);
	session_event_assign(i_ev, plugin_ctx->st, sess, 0, simple_mq_plugin_entry, plugin_ctx);
	printf("%s(plug:%d)session_event_assign-----------%20s: \n", __FUNCTION__, plugin_ctx->plugin_id, session_get0_readable_addr(sess));
	return 0;
}

static void simple_exdata_free(struct session *sess, int idx, void *ex_ptr, void *arg)
{
	if(ex_ptr)
	{
		FREE(ex_ptr);
	}
	return;
}

void *simple_stellar_event_plugin_init(struct stellar *st)
{
	struct simple_stellar_plugin_ctx *ctx = CALLOC(struct simple_stellar_plugin_ctx, 1);
	ctx->st = st;
	ctx->exdata_idx = stellar_session_get_ex_new_index(st, "SIMPLE_EVENT_PLUGIN", simple_exdata_free, NULL);
	int plugin_id=stellar_plugin_register(st, (SESS_EV_TCP|SESS_EV_UDP|SESS_EV_OPENING|SESS_EV_PACKET|SESS_EV_CLOSING), simple_event_plugin_entry, ctx);
	if(plugin_id >= 0)
	{
		ctx->plugin_id=plugin_id;
	}
	return ctx;
}

void simple_stellar_event_plugin_exit(void *ctx)
{
	if(ctx)FREE(ctx);
	return;
}

void *simple_stellar_mq_plugin_init(struct stellar *st)
{
	struct simple_stellar_plugin_ctx *ctx = CALLOC(struct simple_stellar_plugin_ctx, 1);
	ctx->st = st;
	ctx->exdata_idx = stellar_session_get_ex_new_index(st, "SIMPLE_MQ_PLUGIN", simple_exdata_free, NULL);	
	int topic_id=session_mq_get_topic_id(st, "SIMPLE_MQ_TOPIC");
	if(topic_id < 0)
	{
		topic_id=session_mq_create_topic(st, "SIMPLE_MQ_TOPIC", NULL, NULL);
	}
	session_mq_subscribe_topic(st, topic_id, session_mq_plugin_sub_fn, ctx);
	int plugin_id=stellar_plugin_register(st, 0, simple_mq_plugin_entry, ctx);
	if(plugin_id >= 0)
	{
		ctx->plugin_id=plugin_id;
	}
	return ctx;
}

void simple_stellar_mq_plugin_exit(void *ctx)
{
	if(ctx)FREE(ctx);
	return;
}