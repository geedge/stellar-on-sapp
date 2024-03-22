#include "src/adapter/stellar_internal.h"
#include "stellar/stellar.h"
#include "stellar/utils.h"
#include "stellar/session_exdata.h"
#include "stellar/session_mq.h"

#include <stdio.h>

struct simple_stellar_plugin_env
{
    int plugin_id;
    int exdata_idx;
	int topic_id;
	int sub_id;
	struct stellar *st;
};

struct simple_message
{
	uint32_t c2s_pkts;
	uint32_t c2s_bytes;
	uint32_t s2c_pkts;
	uint32_t s2c_bytes;
	struct session_event* ud_ev;
};


static void print_simple_message(struct session *sess, struct simple_message *msg, int plugin_id)
{
	printf("%s(plug:%d)-----------%20s: ", __FUNCTION__, plugin_id, session_get0_readable_addr(sess));
	printf("server-pkt=%u, server-count=%u, client-pkt=%u, client-count=%u, ",
	msg->c2s_pkts, msg->c2s_bytes,
	msg->s2c_pkts, msg->s2c_bytes);
	printf("total-pkt=%u, ", msg->c2s_pkts+msg->s2c_pkts);
	printf("total-count=%u\n", msg->c2s_bytes+msg->s2c_bytes);
	return;
}

static void simple_message_free(void *data, void *cb_arg)
{
	FREE(data);
	return;
}

static void simple_exdata_free(struct session *sess, int idx, void *ex_ptr, void *arg)
{
	if(ex_ptr)
	{
		FREE(ex_ptr);
	}
	return;
}

/***************************************
 * simple stellar mq publisher plugin  *
 ***************************************/

int simple_event_plugin_user_defined_ev(struct session *sess, int events, const struct packet *pkt, void *cb_arg)
{
	if(cb_arg == NULL)return -1;
	struct simple_stellar_plugin_env *plugin_env=(struct simple_stellar_plugin_env *)cb_arg;
	struct simple_message *msg = (struct simple_message *)session_get_ex_data(sess, plugin_env->exdata_idx);
	printf("%s(plug:%d)trigger-----------%20s\n", __FUNCTION__, plugin_env->plugin_id, session_get0_readable_addr(sess));
	print_simple_message(sess, msg, plugin_env->plugin_id);
	return 0;
}

static struct simple_message *simple_message_dup(struct simple_message *in)
{
	struct simple_message *out = CALLOC(struct simple_message, 1);
	if(out)
	{
		out->c2s_pkts = in->c2s_pkts;
		out->c2s_bytes = in->c2s_bytes;
		out->s2c_pkts = in->s2c_pkts;
		out->s2c_bytes = in->s2c_bytes;
	}
	return out;
}

int simple_event_plugin_entry(struct session *sess, int events, const struct packet *pkt, void *cb_arg)
{
	if(cb_arg == NULL)return -1;
	struct simple_stellar_plugin_env *plugin_env=(struct simple_stellar_plugin_env *)cb_arg;
	struct simple_message *exdata_msg = (struct simple_message *)session_get_ex_data(sess, plugin_env->exdata_idx);
	if (exdata_msg == NULL)
	{
		exdata_msg = CALLOC(struct simple_message, 1);
		session_set_ex_data(sess, plugin_env->exdata_idx, exdata_msg);
	}

 	if ((events & SESS_EV_OPENING))
	{
		exdata_msg->ud_ev=session_event_new(plugin_env->st, sess, (SESS_EV_TCP|SESS_EV_UDP|SESS_EV_OPENING|SESS_EV_PACKET|SESS_EV_CLOSING), simple_event_plugin_user_defined_ev, cb_arg);
		session_event_add(exdata_msg->ud_ev, NULL);
	}

	if (pkt)
	{
        size_t payload_len = 0;
        session_get0_current_payload(sess, &payload_len);
        int dir = session_get_direction(sess);
		if (dir==SESSION_DIRECTION_IN)
		{
			exdata_msg->c2s_bytes += payload_len;
			exdata_msg->c2s_pkts += 1;
		}
		if (dir==SESSION_DIRECTION_OUT)
		{
			exdata_msg->s2c_bytes += payload_len;
			exdata_msg->s2c_pkts += 1;
		}
		struct simple_message *send_msg = simple_message_dup(exdata_msg);
		if(session_mq_publish_message(sess, plugin_env->topic_id, send_msg) < 0)
		{
			FREE(send_msg);
		}
	}
    if (exdata_msg != NULL && (events & SESS_EV_CLOSING))
    {
        print_simple_message(sess, exdata_msg, plugin_env->plugin_id);
		session_event_free(exdata_msg->ud_ev);
    }
    return 0;
}

void *simple_stellar_event_plugin_init(struct stellar *st)
{
	struct simple_stellar_plugin_env *plugin_env = CALLOC(struct simple_stellar_plugin_env, 1);
	plugin_env->st = st;
	plugin_env->exdata_idx = stellar_session_get_ex_new_index(st, "SIMPLE_EVENT_PLUGIN", simple_exdata_free, NULL);
	if(plugin_env->exdata_idx < 0)exit(-1);
	int plugin_id=stellar_plugin_register(st, (SESS_EV_TCP|SESS_EV_UDP|SESS_EV_OPENING|SESS_EV_PACKET|SESS_EV_CLOSING), simple_event_plugin_entry, plugin_env);
	if(plugin_id < 0)exit(-1);
	plugin_env->plugin_id=plugin_id;

	session_mq_create_topic(st, "SIMPLE_MQ_TOPIC", simple_message_free, NULL);
	plugin_env->topic_id=session_mq_get_topic_id(st, "SIMPLE_MQ_TOPIC");
	if(plugin_env->topic_id < 0)exit(-1);	
	session_mq_update_topic(st, plugin_env->topic_id, simple_message_free, NULL);
	return plugin_env;
}

void simple_stellar_event_plugin_exit(void *plugin_env)
{

	if(plugin_env)
	{
		struct simple_stellar_plugin_env *env=(struct simple_stellar_plugin_env *)plugin_env;
		session_mq_destroy_topic(env->st, env->topic_id);
		FREE(plugin_env);
	}
	return;
}


/**************************************
 * simple stellar mq subscibe plugin  *
 **************************************/

int simple_mq_plugin_entry(struct session *sess, int events, const struct packet *pkt, void *cb_arg)
{
	if(cb_arg == NULL)return -1;
	struct simple_stellar_plugin_env *plugin_env=(struct simple_stellar_plugin_env *)cb_arg;
	struct session_event *i_ev=session_get_intrinsic_event(sess, plugin_env->plugin_id);
	session_event_assign(i_ev, plugin_env->st, sess, 0, simple_mq_plugin_entry, plugin_env);// in event entry, remove all subsequent events
	printf("%s(plug:%d)session_event_assign-----------%20s: \n", __FUNCTION__, plugin_env->plugin_id, session_get0_readable_addr(sess));
	return 0;
}

static int session_mq_plugin_sub_fn(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
	struct simple_message *msg =(struct simple_message *)data;	
	struct simple_stellar_plugin_env *plugin_ctx = (struct simple_stellar_plugin_env *)cb_arg;
	printf("%s(topic:%d->plug:%d)-----------%20s: ", __FUNCTION__, topic_id, plugin_ctx->plugin_id, session_get0_readable_addr(sess));
	printf("server-pkt=%u, server-count=%u, client-pkt=%u, client-count=%u, ",
	msg->c2s_pkts, msg->c2s_bytes,
	msg->s2c_pkts, msg->s2c_bytes);
	printf("total-pkt=%u, ", msg->c2s_pkts+msg->s2c_pkts);
	printf("total-count=%u\n", msg->c2s_bytes+msg->s2c_bytes);

	if((msg->c2s_pkts+msg->s2c_pkts)>10)
	{
		session_mq_ignore_message(sess, topic_id, plugin_ctx->sub_id);
		printf("%s(topic:%d->plug:%d)session_mq_ignore_message-----------%20s\n", __FUNCTION__, topic_id, plugin_ctx->plugin_id, session_get0_readable_addr(sess));
	}
	else
	{
		struct session_event *i_ev=session_get_intrinsic_event(sess, plugin_ctx->plugin_id);
		session_event_assign(i_ev, plugin_ctx->st, sess, (SESS_EV_TCP|SESS_EV_UDP|SESS_EV_OPENING|SESS_EV_PACKET|SESS_EV_CLOSING), simple_mq_plugin_entry, plugin_ctx);
		printf("%s(plug:%d)session_event_assign-----------%20s\n", __FUNCTION__, plugin_ctx->plugin_id, session_get0_readable_addr(sess));
	}

	return 0;
}



void *simple_stellar_mq_plugin_init(struct stellar *st)
{
	struct simple_stellar_plugin_env *plugin_env = CALLOC(struct simple_stellar_plugin_env, 1);
	plugin_env->st = st;
	plugin_env->exdata_idx = stellar_session_get_ex_new_index(st, "SIMPLE_MQ_PLUGIN", simple_exdata_free, NULL);	
	session_mq_create_topic(st, "SIMPLE_MQ_TOPIC", simple_message_free, NULL);
	plugin_env->topic_id=session_mq_get_topic_id(st, "SIMPLE_MQ_TOPIC");
	if(plugin_env->topic_id < 0)exit(-1);
	plugin_env->sub_id=session_mq_subscribe_topic(st, plugin_env->topic_id, session_mq_plugin_sub_fn, plugin_env);
	int plugin_id=stellar_plugin_register(st, 0, simple_mq_plugin_entry, plugin_env);
	if(plugin_id < 0)exit(-1);
	plugin_env->plugin_id=plugin_id;
	return plugin_env;
}

void simple_stellar_mq_plugin_exit(void *plugin_env)
{
	if(plugin_env)FREE(plugin_env);
	return;
}

/*********************************************
 * simple stellar pakcet and polling plugin  *
 *********************************************/

struct pp_plugin_env
{
	struct stellar *st;
	long long polling_cnt[256];
	long long tcp_pkt_cnt[256];
	long long udp_pkt_cnt[256]; 
};


static int simple_pp_plugin_on_polling_entry(void *cb_arg)
{
	struct pp_plugin_env *plugin_env=(struct pp_plugin_env *)cb_arg;
	int work_thread_idx=stellar_get_current_thread_id(plugin_env->st);
	plugin_env->polling_cnt[work_thread_idx]+=1; 
	return 0;
}

static void simple_pp_plugin_on_packet_entry(struct packet *pkt,  unsigned char ip_protocol, void *cb_arg)
{
	struct pp_plugin_env *plugin_env=(struct pp_plugin_env *)cb_arg;
	int work_thread_idx=stellar_get_current_thread_id(plugin_env->st);
	switch (ip_protocol)
	{
	case 6:
		plugin_env->tcp_pkt_cnt[work_thread_idx]+=1;
		break;
	case 17:
		plugin_env->udp_pkt_cnt[work_thread_idx]+=1;
		break;
	default:
		break;
	}
}

void *simple_stellar_pp_plugin_init(struct stellar *st)
{
	struct pp_plugin_env *plugin_env = CALLOC(struct pp_plugin_env, 1);
	plugin_env->st = st;
	stellar_polling_plugin_register(st,  simple_pp_plugin_on_polling_entry, plugin_env);
	stellar_packet_plugin_register(st, 6, simple_pp_plugin_on_packet_entry, plugin_env);
	stellar_packet_plugin_register(st, 17, simple_pp_plugin_on_packet_entry, plugin_env);
	return plugin_env;
}

void simple_stellar_pp_plugin_exit(void *plugin_env)
{
	struct pp_plugin_env *pp_env=(struct pp_plugin_env *)plugin_env;
	int thread_num=stellar_get_worker_thread_num(pp_env->st);
	printf("(%s)-----------thread_num:%d\n", __FUNCTION__, thread_num);
	for(int i=0; i<thread_num; i++)
	{
		printf("thread:%d, polling_cnt:%lld, tcp_pkt_cnt:%lld, udp_pkt_cnt:%lld\n", i, pp_env->polling_cnt[i], pp_env->tcp_pkt_cnt[i], pp_env->udp_pkt_cnt[i]);
	}
	if(plugin_env)FREE(plugin_env);
	return;
}