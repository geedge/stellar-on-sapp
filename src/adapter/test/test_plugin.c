#include "plugin_spec.h"
#include "session.h"
#include "utils.h"
#include "test_plugin.h"

#include <stdio.h>

int g_test_entry_plugin_id = 0;
int g_test_mq_plugin_id = 0;


int g_test_entry_exdata_idx = 0;
int g_test_mq_plugin_exdata_idx = 0;

struct test_session_message
{
	uint32_t c2s_pkts;
	uint32_t c2s_bytes;
	uint32_t s2c_pkts;
	uint32_t s2c_bytes;
};

static void test_exdata_free(struct session *sess, int idx, void *ex_ptr, void *arg)
{
	if(ex_ptr)
	{
		FREE(ex_ptr);
	}
	return;
}

int test_entry_plugin_init(int plugin_id)
{
	g_test_entry_plugin_id = plugin_id;
	g_test_entry_exdata_idx=session_get_ex_new_index("TEST_ENTRY", test_exdata_free, NULL);
	return 0;
}
void test_entry_plugin_exit(void)
{
	return;
}

static int  test_session_mq_plugin_read(struct session *sess, const char* topic_name, const void *data, void *cb_arg)
{
	struct test_session_message *ctx =(struct test_session_message *)data;	
	printf("plugin_read_message(%s)-----------%20s", topic_name, session_get_readable_addr(sess));
	printf("server-pkt=%u, server-count=%u, client-pkt=%u, client-count=%u, ",
	ctx->c2s_pkts, ctx->c2s_bytes,
	ctx->s2c_pkts, ctx->s2c_bytes);
	printf("total-pkt=%u, ", ctx->c2s_pkts+ctx->s2c_pkts);
	printf("total-count=%u\n", ctx->c2s_bytes+ctx->s2c_bytes);
	printf("session(%d)-----------%20s: attach\n", g_test_mq_plugin_id, session_get_readable_addr(sess));
	session_attach_plugin(sess, g_test_mq_plugin_id);
	return 0;
}

int test_mq_plugin_init(int plugin_id)
{
	g_test_mq_plugin_id = plugin_id;
	g_test_mq_plugin_exdata_idx=session_get_ex_new_index("TEST_MQ", test_exdata_free, NULL);
	session_mq_subscribe_topic("test_mq_topic", test_session_mq_plugin_read, NULL, g_test_mq_plugin_id);
	return 0;
}
void test_mq_plugin_exit(void)
{
	return;
}



static void print_session_ctx(struct session *sess, struct test_session_message *ctx, int plugin_id)
{
	printf("session(%d)-----------%20s: ", plugin_id, session_get_readable_addr(sess));
	printf("server-pkt=%u, server-count=%u, client-pkt=%u, client-count=%u, ",
	ctx->c2s_pkts, ctx->c2s_bytes,
	ctx->s2c_pkts, ctx->s2c_bytes);
	printf("total-pkt=%u, ", ctx->c2s_pkts+ctx->s2c_pkts);
	printf("total-count=%u\n", ctx->c2s_bytes+ctx->s2c_bytes);
	return;
}

void test_entry_plugin_entry(struct session *sess, enum session_state state, int thread_id)
{
	struct test_session_message *test_ctx = session_get_ex_data(sess, g_test_entry_exdata_idx);
	if (test_ctx == NULL)
	{
		test_ctx = CALLOC(struct test_session_message, 1);
		session_set_ex_data(sess, g_test_entry_exdata_idx, test_ctx);
	}
	size_t payload_len = 0;
	session_get_current_payload(sess, &payload_len);
	const struct packet *pkt = session_get_current_packet(sess);
	if (pkt)
	{
		struct flow_flag flag = session_get_current_flow_flag(sess);
		if (flag.flow_flag_see_client_to_server)
		{
			test_ctx->c2s_bytes += payload_len;
			test_ctx->c2s_pkts += 1;
		}
		if (flag.flow_flag_see_server_to_client)
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
			print_session_ctx(sess, test_ctx, g_test_entry_plugin_id);
			FREE(test_ctx);
		}
		break;
	default:
		break;
	}
	return;
}

void test_mq_plugin_entry(struct session *sess, enum session_state state, int thread_id)
{
	printf("session(%d)-----------%20s: dettach\n", g_test_mq_plugin_id, session_get_readable_addr(sess));
	session_dettach_plugin(sess, g_test_mq_plugin_id);
	return;
}

