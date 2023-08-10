#include "plugin_spec.h"
#include "session.h"
#include "utils.h"
#include "test_plugin.h"

#include <stdio.h>

int g_test_entry_plugin_id = 0;
int g_test_mq_plugin_id = 0;

struct test_session_message
{
	uint32_t c2s_pkts;
	uint32_t c2s_bytes;
	uint32_t s2c_pkts;
	uint32_t s2c_bytes;
};

int test_entry_plugin_init(int plugin_id)
{
	g_test_entry_plugin_id = plugin_id;
	return 0;
}
void test_entry_plugin_exit(void)
{
	return;
}

int  test_session_mq_plugin_read(struct session *sess, const char* topic_name, void *data, void *cb_arg)
{
	struct test_session_message *ctx =(struct test_session_message *)data;	
	printf("plugin_read_message(%s)-----------%20s", topic_name, session_printaddr(sess));
	printf("server-pkt=%u, server-count=%u, client-pkt=%u, client-count=%u, ",
	ctx->c2s_pkts, ctx->c2s_bytes,
	ctx->s2c_pkts, ctx->s2c_bytes);
	printf("total-pkt=%u, ", ctx->c2s_pkts+ctx->s2c_pkts);
	printf("total-count=%u\n", ctx->c2s_bytes+ctx->s2c_bytes);
	return 0;
}

int test_mq_plugin_init(int plugin_id)
{
	g_test_mq_plugin_id = plugin_id;
	session_mq_topic_subscribe("test_mq_topic", test_session_mq_plugin_read, NULL);
	return 0;
}
void test_mq_plugin_exit(void)
{
	return;
}



void print_session_ctx(struct session *session, struct test_session_message *ctx, int plugin_id)
{
	printf("session(%d)-----------%20s: ", plugin_id, session_printaddr(session));
	printf("server-pkt=%u, server-count=%u, client-pkt=%u, client-count=%u, ",
	ctx->c2s_pkts, ctx->c2s_bytes,
	ctx->s2c_pkts, ctx->s2c_bytes);
	printf("total-pkt=%u, ", ctx->c2s_pkts+ctx->s2c_pkts);
	printf("total-count=%u\n", ctx->c2s_bytes+ctx->s2c_bytes);
	return;
}

void test_entry_plugin_entry(struct session *session, enum session_state state, int thread_id)
{
	struct test_session_message *test_ctx = session_get_plugin_ctx(session, g_test_entry_plugin_id);
	if (test_ctx == NULL)
	{
		test_ctx = CALLOC(struct test_session_message, 1);
		session_set_plugin_ctx(session, g_test_entry_plugin_id, test_ctx);
	}
	int payload_len = 0;
	int curdir = session_current_direction_get(session);
	session_payload_get(session, &payload_len);
	const struct packet *pkt = session_packet_get(session);
	if (pkt)
	{
		if (curdir == SESSION_DIR_C2S)
		{
			test_ctx->c2s_bytes += payload_len;
			test_ctx->c2s_pkts += 1;
		}
		if (curdir == SESSION_DIR_S2C)
		{
			test_ctx->s2c_bytes += payload_len;
			test_ctx->s2c_pkts += 1;
		}
	}
	switch (state)
	{
	case SESSION_STATE_CLOSING:
		if (test_ctx != NULL)
		{
			print_session_ctx(session, test_ctx, g_test_entry_plugin_id);
			FREE(test_ctx);
		}
		break;
	default:
		break;
	}
	return;
}

void test_mq_plugin_entry(struct session *session, enum session_state state, int thread_id)
{
	return;
}

