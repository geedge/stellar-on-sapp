#include "stellar/stellar.h"
#include "stellar/session.h"
#include "stellar/utils.h"
#include "stellar/session_exdata.h"
#include "stellar/session_mq.h"

#include <stdio.h>
#include <string.h>

#include <assert.h>

struct simple_stellar_plugin_env
{
	struct stellar *st;
    int session_plugin_id;
    int session_exdata_idx;
	int stat_topic_id;
	int egress_topic_id;
	int tcp_topic_id;
	int udp_topic_id;
	int tcp_stream_topic_id;
	long long tcp_packet_count;
	long long udp_packet_count;
	long long icmp_packet_count;
	long long icmp6_packet_count;
	long long polling_times;
};

struct mq_message_stat
{
	uint32_t c2s_pkts;
	uint32_t c2s_bytes;
	uint32_t s2c_pkts;
	uint32_t s2c_bytes;
	uint32_t c2s_stream_pkts;
	uint32_t c2s_stream_bytes;
	uint32_t s2c_stream_pkts;
	uint32_t s2c_stream_bytes;
};


static void print_session_stat(struct session *sess, struct mq_message_stat *stat, int plugin_id, const char *banner)
{
    if (stat)
    {
        printf("%s(plug:%d)-----------%20s: ", banner, plugin_id, session_get0_readable_addr(sess));
        printf("server-pkt=%u, server-count=%u, client-pkt=%u, client-count=%u, ", stat->c2s_pkts, stat->c2s_bytes,
            stat->s2c_pkts, stat->s2c_bytes);
        printf("total-pkt=%u, ", stat->c2s_pkts + stat->s2c_pkts);
        printf("total-count=%u\n", stat->c2s_bytes + stat->s2c_bytes);
		if(stat->c2s_stream_pkts+stat->s2c_stream_pkts > 0)
		{
        	printf("----------------server-stream_pkt=%u, server-stream_count=%u, client-stream_pkt=%u, client-stream_count=%u\n", stat->c2s_stream_pkts, stat->c2s_stream_bytes,
            stat->s2c_stream_pkts, stat->s2c_stream_bytes);			
		}
    }
    return;
}

/*******************************
 * simple session & packet plugin *
 *******************************/

static void *simple_plugin_session_ctx_new(struct session *sess, void *plugin_env)
{
	struct mq_message_stat * stat= CALLOC(struct mq_message_stat, 1);
	struct simple_stellar_plugin_env *env=(struct simple_stellar_plugin_env *)plugin_env;
	session_exdata_set(sess, env->session_exdata_idx, stat);
	return stat;
}

static void simple_plugin_session_ctx_free(struct session *sess, void *session_ctx, void *plugin_env)
{
	struct simple_stellar_plugin_env *env=(struct simple_stellar_plugin_env *)plugin_env;
	struct mq_message_stat *stat = (struct mq_message_stat *)session_ctx;
	print_session_stat(sess, stat, env->session_plugin_id, __FUNCTION__);
	session_exdata_set(sess, env->session_exdata_idx, NULL);
	if(session_ctx)FREE(session_ctx);
	return;
}

static void simple_plugin_on_session_msg(struct session *sess, int topic_id, const void *data, void *plugin_ctx, void *plugin_env)
{
	struct simple_stellar_plugin_env *env = (struct simple_stellar_plugin_env *)plugin_env;
	struct mq_message_stat *stat = (struct mq_message_stat *)plugin_ctx;
	struct packet * pkt=(struct packet *)data;
	if (pkt)
	{
		// TEST: try stellar_session_plugin_dettach_current_session when pkt > 3
		if(stat->c2s_pkts+stat->s2c_pkts >= 3 && session_get_type(sess)== SESSION_TYPE_UDP)
		{
			stellar_session_plugin_dettach_current_session(sess);
			return;
		}

        size_t payload_len = 0;
        session_get0_current_payload(sess, &payload_len);
        int dir = packet_get_direction(pkt);
		if (dir==PACKET_DIRECTION_C2S)
		{
			if(topic_id==env->tcp_stream_topic_id)
			{
				stat->c2s_stream_bytes += payload_len;
				stat->c2s_stream_pkts += 1;
			}
			else
			{
				stat->c2s_bytes += payload_len;
				stat->c2s_pkts += 1;
			}
		}
		if (dir==PACKET_DIRECTION_S2C)
		{
			if(topic_id==env->tcp_stream_topic_id)
			{
				stat->s2c_stream_bytes += payload_len;
				stat->s2c_stream_pkts += 1;
			}
			else
			{
				stat->s2c_bytes += payload_len;
				stat->s2c_pkts += 1;
			}
		}
		session_mq_publish_message(sess, ((struct simple_stellar_plugin_env *)plugin_env)->stat_topic_id, stat);
	}
    return;
}

void simple_plugin_on_packet(struct packet *pkt,  unsigned char ip_protocol, void *plugin_env)
{
	struct simple_stellar_plugin_env *env = (struct simple_stellar_plugin_env *)plugin_env;
	switch (ip_protocol)
	{
	case IPPROTO_TCP:
		env->tcp_packet_count++;
		break;
	case IPPROTO_UDP:
		env->udp_packet_count++;
		break;	
	case IPPROTO_ICMP:
		env->icmp_packet_count++;
		break;			
	case IPPROTO_ICMPV6:
		env->icmp6_packet_count++;
		break;		
	default:
		perror("invalid ip_protocol\n");
		exit(-1);
		break;
	}
	return;
}

int simple_plugin_on_polling(void *plugin_env)
{
	struct simple_stellar_plugin_env *env = (struct simple_stellar_plugin_env *)plugin_env;
	env->polling_times++;
	return 0;
}

static void simple_plugin_packet_exdata_free(struct packet *pkt, int idx, void *ex_ptr, void *arg)
{
	struct simple_stellar_plugin_env *env = (struct simple_stellar_plugin_env *)arg;	
	assert(env);
	struct simple_stellar_plugin_env *exdata = (struct simple_stellar_plugin_env *)ex_ptr;		
	assert(memcmp(env, exdata, sizeof(struct simple_stellar_plugin_env)) == 0);
}

static void simple_plugin_packet_msg_free(struct packet *pkt, void *msg, void *msg_free_arg)
{
	struct simple_stellar_plugin_env *env = (struct simple_stellar_plugin_env *)msg_free_arg;	
	assert(env);
	struct simple_stellar_plugin_env *exdata = (struct simple_stellar_plugin_env *)msg;		
	assert(memcmp(env, exdata, sizeof(struct simple_stellar_plugin_env)) == 0);	
}

static void simple_plugin_on_packet_msg_cb(struct packet *pkt, int topic_id, const void *msg, void *plugin_env)
{
	struct simple_stellar_plugin_env *env = (struct simple_stellar_plugin_env *)plugin_env;	
	assert(env);
	struct simple_stellar_plugin_env *exdata = (struct simple_stellar_plugin_env *)msg;		
	assert(memcmp(env, exdata, sizeof(struct simple_stellar_plugin_env)) == 0);
}

void *simple_session_packet_plugin_init(struct stellar *st)
{
	struct simple_stellar_plugin_env *env = CALLOC(struct simple_stellar_plugin_env, 1);
	env->st = st;
	env->session_exdata_idx = stellar_session_exdata_new_index(st, "EXDATA_SESSION_STAT", NULL, NULL);
	env->session_plugin_id = stellar_session_plugin_register(st, 
									simple_plugin_session_ctx_new, 
									simple_plugin_session_ctx_free, 
									env);
	int tcp_plugin_id=stellar_packet_plugin_register(st, IPPROTO_TCP, simple_plugin_on_packet, env);	
	int udp_plugin_id=stellar_packet_plugin_register(st, IPPROTO_UDP, simple_plugin_on_packet, env);	
	int icmp_plugin_id=stellar_packet_plugin_register(st, IPPROTO_ICMP, simple_plugin_on_packet, env);	
	int icmp6_plugin_id=stellar_packet_plugin_register(st, IPPROTO_ICMPV6, simple_plugin_on_packet, env);	

	if(tcp_plugin_id < 0 || udp_plugin_id < 0 || icmp_plugin_id < 0 || icmp6_plugin_id < 0)
	{
		perror("register packet plugin return invalid plugin id\n");
		exit(-1);
	}

	int polling_plugin_id=stellar_polling_plugin_register(st, simple_plugin_on_polling, env);
	if(polling_plugin_id < 0)
	{
		perror("register polling plugin return invalid plugin id \n");
		exit(-1);
	}

	env->tcp_stream_topic_id=stellar_session_mq_get_topic_id(st, TOPIC_TCP_STREAM);
	env->tcp_topic_id=stellar_session_mq_get_topic_id(st, TOPIC_TCP);
	env->udp_topic_id=stellar_session_mq_get_topic_id(st, TOPIC_UDP);
	if(env->tcp_topic_id < 0 || env->udp_topic_id < 0 || env->tcp_stream_topic_id < 0)
	{
		perror("get tcp or udp topic id failed\n");
		exit(-1);
	}

	stellar_session_mq_subscribe(st, env->tcp_stream_topic_id, simple_plugin_on_session_msg, env->session_plugin_id);
	stellar_session_mq_subscribe(st, env->tcp_topic_id, simple_plugin_on_session_msg, env->session_plugin_id);
	stellar_session_mq_subscribe(st, env->udp_topic_id, simple_plugin_on_session_msg, env->session_plugin_id);

	int stat_topic_id=stellar_session_mq_get_topic_id(st, "TOPIC_SESSION_STAT");
	if(stat_topic_id < 0)
	{
		stat_topic_id=stellar_session_mq_create_topic(st, "TOPIC_SESSION_STAT", NULL, NULL);
	}	
	env->stat_topic_id = stat_topic_id;								
	return env;
}

void simple_session_packet_plugin_exit(void *plugin_env)
{
	if(plugin_env)
	{
		struct simple_stellar_plugin_env *env = (struct simple_stellar_plugin_env *)plugin_env;
		printf("(%s):tcp_packet_num:%lld, udp_packet_num:%lld, icmp_packet_num:%lld, icmp6_packet_num:%lld, polling_times:%lld\n", __FUNCTION__, env->tcp_packet_count, env->udp_packet_count, env->icmp_packet_count, env->icmp6_packet_count, env->polling_times);
		FREE(plugin_env);
	}
	return;
}

/*******************************
 * simple plugin sub session stat  *
 *******************************/

static void simple_plugin_sub_session_stat_on_msg(struct session *sess, int topic_id, const void *data, void *plugin_ctx, void *plugin_env)
{

	struct simple_stellar_plugin_env *env = (struct simple_stellar_plugin_env *)plugin_env;
	if(topic_id == env->egress_topic_id)
	{
		session_mq_ignore_message(sess, topic_id, env->session_plugin_id);
	}
    if (topic_id == env->stat_topic_id)
    {
        struct mq_message_stat *stat = (struct mq_message_stat *)data;
        struct mq_message_stat *exdata_stat = (struct mq_message_stat *)session_exdata_get(sess, env->session_exdata_idx);
        if (memcmp(exdata_stat, stat, sizeof(struct mq_message_stat)) != 0)
        {
            perror("exdata and mq data not equal\n");
            exit(-1);
        }
        // print_session_stat(sess, stat, env->plugin_id, __FUNCTION__);
		session_mq_unignore_message(sess,  env->egress_topic_id, env->session_plugin_id);
    }
    return;
}

void *simple_plugin_sub_session_stat_init(struct stellar *st)
{
	struct simple_stellar_plugin_env *env = CALLOC(struct simple_stellar_plugin_env, 1);
	env->st = st;
	env->session_exdata_idx = stellar_session_exdata_new_index(st, "EXDATA_SESSION_STAT", NULL, NULL);
	int topic_id=stellar_session_mq_get_topic_id(st, "TOPIC_SESSION_STAT");
	if(topic_id < 0)
	{
		topic_id=stellar_session_mq_create_topic(st, "TOPIC_SESSION_STAT", NULL, NULL);
	}
	env->stat_topic_id = topic_id;								
	env->session_plugin_id = stellar_session_plugin_register(st, 
									NULL, 
									NULL, 
									env);
	stellar_session_mq_subscribe(st, topic_id, simple_plugin_sub_session_stat_on_msg, env->session_plugin_id);

	// TEST: subscirbe egress message then ignore
	env->egress_topic_id=stellar_session_mq_get_topic_id(st, TOPIC_EGRESS);
	if(env->egress_topic_id < 0)
	{
		perror("get egress topic id failed\n");
		exit(-1);
	}
	stellar_session_mq_subscribe(st, env->egress_topic_id, simple_plugin_sub_session_stat_on_msg, env->session_plugin_id);
	return env;
}

void simple_plugin_sub_session_stat_exit(void *plugin_env)
{
	if(plugin_env)FREE(plugin_env);
	return;
}