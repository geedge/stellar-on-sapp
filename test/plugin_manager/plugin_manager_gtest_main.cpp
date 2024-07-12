#include <gtest/gtest.h>

#include "stellar/utils.h"
#include "plugin_manager_gtest_mock.h"

void whitebox_test_plugin_manager_intrisic_metadata(struct stellar *st, struct plugin_manager_schema *plug_mgr)
{
	SCOPED_TRACE("whitebox test intrisic metadata"); 

	EXPECT_TRUE(plug_mgr!=NULL);

	EXPECT_EQ(plug_mgr->st, st);

	//load spec null
	EXPECT_TRUE(plug_mgr->plugin_load_specs_array==NULL);

	//packet exdata & mq schema null

	//session exdata schema null
	EXPECT_TRUE(plug_mgr->session_exdata_schema_array==NULL);

	//session mq schema not null
	EXPECT_TRUE(plug_mgr->session_mq_schema_array!=NULL);

	//registered plugin array null
	EXPECT_TRUE(plug_mgr->registered_polling_plugin_array==NULL);
	EXPECT_TRUE(plug_mgr->registered_packet_plugin_array==NULL);
	EXPECT_TRUE(plug_mgr->registered_session_plugin_array==NULL);

	int intrinsic_topic_num=utarray_len(plug_mgr->session_mq_schema_array);
	EXPECT_EQ(plug_mgr->session_mq_topic_num, intrinsic_topic_num);//TCP,UDP,TCP_STREAM,EGRESS,CONTROL

	struct stellar_mq_topic_schema *topic = (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr->session_mq_schema_array, (unsigned int)plug_mgr->tcp_topic_id);
	EXPECT_STREQ(topic->topic_name, TOPIC_TCP);

	topic = (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr->session_mq_schema_array, (unsigned int)plug_mgr->tcp_stream_topic_id);
	EXPECT_STREQ(topic->topic_name, TOPIC_TCP_STREAM);

	topic = (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr->session_mq_schema_array, (unsigned int)plug_mgr->udp_topic_id);
	EXPECT_STREQ(topic->topic_name, TOPIC_UDP);

	topic = (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr->session_mq_schema_array, (unsigned int)plug_mgr->egress_topic_id);
	EXPECT_STREQ(topic->topic_name, TOPIC_EGRESS);

	topic = (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr->session_mq_schema_array, (unsigned int)plug_mgr->control_packet_topic_id);
	EXPECT_STREQ(topic->topic_name, TOPIC_CONTROL_PACKET);

	//intrinsic topic
	EXPECT_GE(stellar_session_mq_get_topic_id(st, TOPIC_TCP), 0);
	EXPECT_GE(stellar_session_mq_get_topic_id(st, TOPIC_TCP_STREAM), 0);
	EXPECT_GE(stellar_session_mq_get_topic_id(st, TOPIC_UDP), 0);
	EXPECT_GE(stellar_session_mq_get_topic_id(st, TOPIC_EGRESS), 0);
	EXPECT_GE(stellar_session_mq_get_topic_id(st, TOPIC_CONTROL_PACKET), 0);	
}

/***********************************
 * TEST PLUGIN MANAGER INIT & EXIT *
 ***********************************/

//TODO: test plugin_specs_load

TEST(plugin_manager_init, init_with_null_toml) {

	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);
	plugin_manager_exit(plug_mgr);
}

/*******************************************
 * TEST PLUGIN MANAGER PACKET PLUGIN RUNTIME*
 *******************************************/

#define PACKET_PROTO_PLUGIN_NUM 128
struct packet_plugin_env
{
	struct plugin_manager_schema *plug_mgr;
	int basic_on_packet_called;
	int proto_filter_plugin_id[PACKET_PROTO_PLUGIN_NUM];
	int proto_filter_plugin_called[PACKET_PROTO_PLUGIN_NUM];
};

static void test_proto_filter_on_packet(struct packet *pkt, unsigned char ip_protocol, void *plugin_env)
{
	struct packet_plugin_env *env = (struct packet_plugin_env *)plugin_env;
	EXPECT_TRUE(env!=NULL);
	EXPECT_EQ(pkt->ip_proto, ip_protocol);
	EXPECT_EQ(pkt->st, env->plug_mgr->st);
	env->proto_filter_plugin_called[ip_protocol]+=1;
	return;
}

TEST(plugin_manager, packet_plugins_with_proto_filter) {

	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

	struct packet_plugin_env env;
	memset(&env, 0, sizeof(struct packet_plugin_env));
	env.plug_mgr=plug_mgr;

	int proto_filter_plugin_num=(int)(sizeof(env.proto_filter_plugin_id) / sizeof(env.proto_filter_plugin_id[0]));
    for (int i = 0; i < proto_filter_plugin_num; i++)
    {
        env.proto_filter_plugin_id[i] = stellar_packet_plugin_register(&st, i, test_proto_filter_on_packet, &env);
        EXPECT_GE(env.proto_filter_plugin_id[i], PACKET_PULGIN_ID_BASE);


    }

	{
        SCOPED_TRACE("White-box test, check stellar internal schema");
        EXPECT_EQ(utarray_len(plug_mgr->registered_packet_plugin_array), proto_filter_plugin_num);
    }

    struct packet pkt={&st, IPv4, 0};

	int N_packet=10;
    for (int j = 0; j < N_packet; j++)
    {
        for (int i = 0; i < proto_filter_plugin_num; i++)
        {
            pkt.ip_proto = i;
            plugin_manager_on_packet_ingress(plug_mgr, &pkt);
        }
    }
    plugin_manager_exit(plug_mgr);

	for (int i = 0; i < proto_filter_plugin_num; i++)
	{
		EXPECT_EQ(env.proto_filter_plugin_called[i], N_packet);
	}
}

/**********************************************
 * TEST PLUGIN MANAGER ON SESSION PLUGIN INIT *
 **********************************************/

static void test_mock_session_exdata_free(struct session *sess, int idx, void *ex_ptr, void *arg){}
static void test_mock_overwrite_session_exdata_free(struct session *sess, int idx, void *ex_ptr, void *arg){}

TEST(plugin_manager_init, session_exdata_new_index_overwrite) {
	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

	const char *exdata_name="SESSION_EXDATA";
	int exdata_idx=stellar_session_exdata_new_index(&st,exdata_name, test_mock_session_exdata_free, &st);
	EXPECT_GE(exdata_idx, 0);
	int overwrite_idx=stellar_session_exdata_new_index(&st,exdata_name, test_mock_overwrite_session_exdata_free, plug_mgr);
	EXPECT_GE(overwrite_idx, 0);
	EXPECT_EQ(overwrite_idx, exdata_idx);

    {
        SCOPED_TRACE("White-box test, check stellar internal schema");
        struct stellar_exdata_schema *exdata_schema = (struct stellar_exdata_schema *)utarray_eltptr(
            plug_mgr->session_exdata_schema_array, (unsigned int)exdata_idx);
        EXPECT_EQ(exdata_schema->free_func, (void *)test_mock_overwrite_session_exdata_free);
        EXPECT_EQ(exdata_schema->free_arg, plug_mgr);
        EXPECT_EQ(exdata_schema->idx, exdata_idx);
        EXPECT_STREQ(exdata_schema->name, exdata_name);

        int exdata_num = utarray_len(plug_mgr->session_exdata_schema_array);
        EXPECT_EQ(exdata_num, 1);
    }
    plugin_manager_exit(plug_mgr);
}

void test_mock_session_msg_free(struct session *sess, void *msg, void *msg_free_arg){}
void test_mock_overwrite_session_msg_free(struct session *sess, void *msg, void *msg_free_arg){}

TEST(plugin_manager_init, session_mq_topic_create_and_update) {
	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

	const char *topic_name="SESSION_TOPIC";

	EXPECT_EQ(stellar_session_mq_get_topic_id(&st, topic_name), -1);// illegal topic_name

	int topic_id=stellar_session_mq_create_topic(&st, topic_name, test_mock_session_msg_free, &st);
	EXPECT_GE(topic_id, 0);
	struct stellar_mq_topic_schema *topic_schema;
    {
        SCOPED_TRACE("White-box test, check stellar internal schema");
        topic_schema =
            (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr->session_mq_schema_array, (unsigned int)topic_id);
        EXPECT_EQ(topic_schema->free_cb, (void *)test_mock_session_msg_free);
        EXPECT_EQ(topic_schema->free_cb_arg, &st);
        EXPECT_EQ(topic_schema->topic_id, topic_id);
        EXPECT_STREQ(topic_schema->topic_name, topic_name);
    }

    EXPECT_EQ(stellar_session_mq_get_topic_id(&st, topic_name), topic_id);
	EXPECT_EQ(stellar_session_mq_create_topic(&st, topic_name, test_mock_overwrite_session_msg_free, plug_mgr), -1); // duplicate create, return error

    {
        SCOPED_TRACE("White-box test, check stellar internal schema");
        topic_schema =
            (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr->session_mq_schema_array, (unsigned int)topic_id);
        EXPECT_EQ(topic_schema->free_cb, (void *)test_mock_session_msg_free);
        EXPECT_EQ(topic_schema->free_cb_arg, &st);
        EXPECT_EQ(topic_schema->topic_id, topic_id);
        EXPECT_STREQ(topic_schema->topic_name, topic_name);
    }

    EXPECT_EQ(stellar_session_mq_update_topic(&st, topic_id, test_mock_overwrite_session_msg_free, plug_mgr), 0);

    {
        SCOPED_TRACE("White-box test, check stellar internal schema");
        topic_schema =
            (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr->session_mq_schema_array, (unsigned int)topic_id);
        EXPECT_EQ(topic_schema->free_cb, (void *)test_mock_overwrite_session_msg_free);
        EXPECT_EQ(topic_schema->free_cb_arg, plug_mgr);
        EXPECT_EQ(topic_schema->topic_id, topic_id);
        EXPECT_STREQ(topic_schema->topic_name, topic_name);

        EXPECT_EQ(utarray_len(plug_mgr->session_mq_schema_array), 6); // 5 intrinsic topic + 1 created topic
    }

	EXPECT_EQ(stellar_session_mq_destroy_topic(&st, 10), -1);// illgeal session topic_id

	EXPECT_EQ(stellar_session_mq_destroy_topic(&st, topic_id), 1);
	EXPECT_EQ(stellar_session_mq_destroy_topic(&st, topic_id), 0);//duplicate destroy, return 0;

	{
		SCOPED_TRACE("White-box test, check stellar internal schema");
		EXPECT_EQ(utarray_len(plug_mgr->session_mq_schema_array), 6);//destory won't delete the topic schema
	}
	EXPECT_EQ(plug_mgr->session_mq_topic_num, 5);//intrinsic topic number

	plugin_manager_exit(plug_mgr);
}

void test_mock_on_session_msg(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env){}
void test_mock_overwrite_on_session_msg(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env){}


TEST(plugin_manager_init, session_mq_subscribe_overwrite) {

	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

	const char *topic_name="SESSION_TOPIC";

	int topic_id=stellar_session_mq_create_topic(&st, topic_name, test_mock_session_msg_free, &st);
	EXPECT_GE(topic_id, 0);

	EXPECT_EQ(stellar_session_mq_subscribe(&st, topic_id, test_mock_on_session_msg, 10),-1);//illgeal plugin_id
	EXPECT_EQ(stellar_session_mq_subscribe(&st, 10, test_mock_on_session_msg, 10),-1);//illgeal topic_id & plugin_id

	int plugin_id=stellar_session_plugin_register(&st, NULL, NULL, &st);
	EXPECT_GE(plugin_id, 0);

	EXPECT_EQ(stellar_session_mq_subscribe(&st, topic_id, test_mock_on_session_msg, plugin_id),0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, topic_id, test_mock_overwrite_on_session_msg, plugin_id),0);//duplicate subscribe, return 0, won't overwrite 

	struct stellar_mq_topic_schema *topic_schema;
	{
		SCOPED_TRACE("White-box test, check stellar internal schema");
		topic_schema = (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr->session_mq_schema_array,(unsigned int)topic_id);
		EXPECT_EQ(topic_schema->free_cb, (void *)test_mock_session_msg_free);
		EXPECT_EQ(topic_schema->free_cb_arg, &st);
		EXPECT_EQ(topic_schema->topic_id, topic_id);
		EXPECT_STREQ(topic_schema->topic_name, topic_name);
	}

	EXPECT_EQ(topic_schema->subscriber_cnt, 1);
	EXPECT_EQ(topic_schema->subscribers->sess_msg_cb, (void *)test_mock_overwrite_on_session_msg);

	plugin_manager_exit(plug_mgr);
}
/**********************************************
 * TEST PLUGIN MANAGER ON SESSION PLUGIN RUNTIME *
 **********************************************/

struct session_plugin_env
{
	struct plugin_manager_schema *plug_mgr;
	int N_session;
	int N_per_session_pkt_cnt;
	int intrinsc_tcp_topic_id;
	int intrinsc_egress_topic_id;
	int basic_exdata_idx;
	int basic_exdata_free_called;
	int basic_on_session_ingress_called;
	int basic_on_session_egress_called;
	int basic_ctx_new_called;
	int basic_ctx_free_called;
	int test_mq_pub_plugin_id;
	int test_mq_sub_plugin_id;
	int test_mq_pub_called;
	int test_mq_sub_called;
	int test_mq_free_called;
	int test_mq_topic_id;
	int plugin_id_1;
	int plugin_id_2;
	int plugin_id_1_called;
	int plugin_id_2_called;
};

TEST(plugin_manager, no_plugin_register_runtime) {

	struct stellar st={0};

// init stage
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

// init plugin

// prepare packet and session

	struct session_plugin_env env;
	memset(&env, 0, sizeof(struct session_plugin_env));
	env.plug_mgr=plug_mgr;
	env.N_per_session_pkt_cnt=10;
	env.N_session=10;

	struct packet pkt={&st, TCP, 6};

	struct session sess[env.N_session];
	memset(&sess, 0, sizeof(sess));

// pesudo running stage
	for(int i=0; i < env.N_session; i++)
		sess[i].plug_mgr_rt=plugin_manager_session_runtime_new(plug_mgr, &sess[i]);

    for (int j = 0; j < env.N_per_session_pkt_cnt; j++)
    {
        plugin_manager_on_packet_ingress(plug_mgr, &pkt);

        for (int i = 0; i < env.N_session; i++)
        {
			sess[i].sess_pkt_cnt+=1;
            plugin_manager_on_session_ingress(&sess[i], &pkt);
            plugin_manager_on_session_egress(&sess[i], &pkt);
        }

    }

    for(int i=0; i < env.N_session; i++)
	{
		plugin_manager_on_session_closing(&sess[i]);
		plugin_manager_session_runtime_free(sess[i].plug_mgr_rt);
	}

//exit stage
	plugin_manager_exit(plug_mgr);
}


struct test_basic_ctx
{
	int called;
};

static void *test_basic_session_ctx_new(struct session *sess, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	env->basic_ctx_new_called+=1;
	struct test_basic_ctx *ctx=CALLOC(struct test_basic_ctx, 1);
	return ctx;
}

static void test_basic_session_ctx_free(struct session *sess, void *session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	env->basic_ctx_free_called+=1;
	struct test_basic_ctx *ctx=(struct test_basic_ctx *)session_ctx;
	EXPECT_EQ(ctx->called, env->N_per_session_pkt_cnt*2);//ingress + egress + closing
	FREE(ctx);
	return;
}

static void test_basic_on_session_ingress(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	struct test_basic_ctx *ctx=(struct test_basic_ctx *)per_session_ctx;
	EXPECT_TRUE(env!=NULL);
	EXPECT_TRUE(ctx!=NULL);
	EXPECT_EQ(sess->plug_mgr_rt->plug_mgr, env->plug_mgr);
	EXPECT_EQ(session_exdata_set(sess, 2, sess), -1);// illegal set
	EXPECT_EQ(session_exdata_get(sess, 2), nullptr);// illegal get
	EXPECT_EQ(session_exdata_set(sess, env->basic_exdata_idx, env), 0);
	if(msg)
	{
		env->basic_on_session_ingress_called+=1;
		ctx->called+=1;
	}
	return;
}

static void test_basic_on_session_egress(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	struct test_basic_ctx *ctx=(struct test_basic_ctx *)per_session_ctx;
	EXPECT_TRUE(env!=NULL);
	EXPECT_TRUE(ctx!=NULL);
	EXPECT_EQ(sess->plug_mgr_rt->plug_mgr, env->plug_mgr);
	EXPECT_EQ(session_exdata_set(sess, 2, sess), -1);// illegal set
	EXPECT_EQ(session_exdata_get(sess, 2), nullptr);// illegal get
	EXPECT_TRUE(msg!=NULL);
	EXPECT_EQ(session_exdata_get(sess, env->basic_exdata_idx), env);
	env->basic_on_session_egress_called+=1;
	ctx->called+=1;
	return;
}

static void test_basic_session_exdata_free(struct session *sess, int idx, void *ex_ptr, void *arg)
{
	struct session_plugin_env *env = (struct session_plugin_env *)arg;
	EXPECT_EQ(env->basic_exdata_idx, idx);
	EXPECT_EQ(ex_ptr, env);
	env->basic_exdata_free_called+=1;
}

TEST(plugin_manager, session_plugin_on_intrinsic_ingress_egress) {

	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

	unsigned char ip_proto=6;
	struct session_plugin_env env;
	memset(&env, 0, sizeof(struct session_plugin_env));
	env.plug_mgr=plug_mgr;
	env.N_per_session_pkt_cnt=10;
	env.N_session=10;

	int plugin_id=stellar_session_plugin_register(&st, test_basic_session_ctx_new, test_basic_session_ctx_free, &env);
	EXPECT_GE(plugin_id, 0);

	env.intrinsc_tcp_topic_id=stellar_session_mq_get_topic_id(&st, TOPIC_TCP);
	EXPECT_GE(env.intrinsc_tcp_topic_id, 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.intrinsc_tcp_topic_id, test_basic_on_session_ingress, plugin_id), 0);

	env.intrinsc_egress_topic_id=stellar_session_mq_get_topic_id(&st, TOPIC_EGRESS);
	EXPECT_GE(env.intrinsc_egress_topic_id, 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.intrinsc_egress_topic_id, test_basic_on_session_ingress, plugin_id), 0);// Intentional error
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.intrinsc_egress_topic_id, test_basic_on_session_egress, plugin_id), 0);

	env.basic_exdata_idx=stellar_session_exdata_new_index(&st, "SESSION_EXDATA", test_basic_session_exdata_free,&env);
	EXPECT_GE(env.basic_exdata_idx, 0);

	struct packet pkt={&st, TCP, ip_proto};


	struct session sess[env.N_session];

	for(int i=0; i < env.N_session; i++)
		sess[i].plug_mgr_rt=plugin_manager_session_runtime_new(plug_mgr, &sess[i]);

    for (int j = 0; j < env.N_per_session_pkt_cnt; j++)
    {
        plugin_manager_on_packet_ingress(plug_mgr, &pkt);

        for (int i = 0; i < env.N_session; i++)
        {
            plugin_manager_on_session_ingress(&sess[i], &pkt);
            plugin_manager_on_session_egress(&sess[i], &pkt);
        }

    }

    for(int i=0; i < env.N_session; i++)
	{
		plugin_manager_on_session_closing(&sess[i]);
		plugin_manager_session_runtime_free(sess[i].plug_mgr_rt);
	}

	plugin_manager_exit(plug_mgr);

	EXPECT_TRUE(env.basic_on_session_ingress_called == env.basic_on_session_egress_called && env.basic_on_session_ingress_called == env.N_session*env.N_per_session_pkt_cnt);
	EXPECT_TRUE(env.basic_ctx_new_called == env.basic_ctx_free_called && env.basic_ctx_new_called == env.N_session);
	EXPECT_EQ(env.basic_exdata_free_called, env.N_session);

}

struct test_session_mq_ctx
{
	int called;
};

static void *test_mq_pub_session_ctx_new(struct session *sess, void *plugin_env)
{
	struct test_basic_ctx *ctx=CALLOC(struct test_basic_ctx, 1);
	return ctx;
}

static void test_mq_pub_session_ctx_free(struct session *sess, void *session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	struct test_basic_ctx *ctx=(struct test_basic_ctx *)session_ctx;
	EXPECT_EQ(ctx->called, env->N_per_session_pkt_cnt);
	FREE(ctx);
	return;
}

static void *test_mq_sub_session_ctx_new(struct session *sess, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	struct test_basic_ctx *ctx=CALLOC(struct test_basic_ctx, 1);
	EXPECT_EQ(session_mq_ignore_message(sess, env->intrinsc_tcp_topic_id, env->test_mq_sub_plugin_id), 0);
	return ctx;
}

static void test_mq_sub_session_ctx_free(struct session *sess, void *session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	struct test_basic_ctx *ctx=(struct test_basic_ctx *)session_ctx;
	EXPECT_EQ(ctx->called, env->N_per_session_pkt_cnt);
	FREE(ctx);
	return;
}

static void test_mq_pub_on_session(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	struct test_basic_ctx *ctx=(struct test_basic_ctx *)per_session_ctx;
	EXPECT_TRUE(env!=NULL);
	EXPECT_TRUE(ctx!=NULL);
	EXPECT_EQ(sess->plug_mgr_rt->plug_mgr, env->plug_mgr);
    if (msg)
    {
        env->test_mq_pub_called += 1;
        ctx->called += 1;
        int *pub_msg = (int *)CALLOC(int, 1);
        *pub_msg = env->test_mq_pub_called;
        EXPECT_EQ(session_mq_publish_message(sess, env->test_mq_topic_id, pub_msg), 0);
    }
    return;
}

static void test_mq_on_sub_msg(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	struct test_basic_ctx *ctx=(struct test_basic_ctx *)per_session_ctx;
	EXPECT_TRUE(env!=NULL);
	EXPECT_TRUE(ctx!=NULL);
	EXPECT_EQ(sess->plug_mgr_rt->plug_mgr, env->plug_mgr);
	EXPECT_EQ(*(int *)msg, env->test_mq_pub_called);
	env->test_mq_sub_called+=1;
	ctx->called+=1;
	return;
}

static void test_session_msg_free(struct session *sess, void *msg, void *msg_free_arg)
{
	struct session_plugin_env *env = (struct session_plugin_env *)msg_free_arg;
	EXPECT_EQ(sess->plug_mgr_rt->plug_mgr, env->plug_mgr);
	if(msg)
	{
		EXPECT_EQ(env->test_mq_pub_called, *(int *)msg);
		env->test_mq_free_called+=1;
		FREE(msg);
	}
	return;
}

TEST(plugin_manager, session_plugin_ignore_on_ctx_new_sub_other_msg) {

	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

	unsigned char ip_proto=6;
	struct session_plugin_env env;
	memset(&env, 0, sizeof(struct session_plugin_env));
	env.plug_mgr=plug_mgr;
	env.N_per_session_pkt_cnt=10;
	env.N_session=10;

	env.test_mq_pub_plugin_id=stellar_session_plugin_register(&st, test_mq_pub_session_ctx_new, test_mq_pub_session_ctx_free, &env);
	EXPECT_GE(env.test_mq_pub_plugin_id, 0);

	env.intrinsc_tcp_topic_id=stellar_session_mq_get_topic_id(&st, TOPIC_TCP);
	EXPECT_GE(env.intrinsc_tcp_topic_id, 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.intrinsc_tcp_topic_id, test_mq_pub_on_session, env.test_mq_pub_plugin_id), 0);

	env.test_mq_topic_id=stellar_session_mq_create_topic(&st, "SESSION_MQ_TOPIC", test_session_msg_free, &env);
	EXPECT_GE(env.test_mq_topic_id, 0);

	env.test_mq_sub_plugin_id=stellar_session_plugin_register(&st, test_mq_sub_session_ctx_new, test_mq_sub_session_ctx_free, &env);
	EXPECT_GE(env.test_mq_sub_plugin_id, 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.test_mq_topic_id, test_mq_on_sub_msg, env.test_mq_sub_plugin_id), 0);

	struct packet pkt={&st, TCP, ip_proto};


	struct session sess[env.N_session];

	for(int i=0; i < env.N_session; i++)
		sess[i].plug_mgr_rt=plugin_manager_session_runtime_new(plug_mgr, &sess[i]);

    for (int j = 0; j < env.N_per_session_pkt_cnt; j++)
    {
        plugin_manager_on_packet_ingress(plug_mgr, &pkt);

        for (int i = 0; i < env.N_session; i++)
        {
            plugin_manager_on_session_ingress(&sess[i], &pkt);
            plugin_manager_on_session_egress(&sess[i], &pkt);
        }

    }

    for(int i=0; i < env.N_session; i++)
	{
		plugin_manager_on_session_closing(&sess[i]);
		plugin_manager_session_runtime_free(sess[i].plug_mgr_rt);
	}

	plugin_manager_exit(plug_mgr);

	EXPECT_TRUE(env.test_mq_free_called == env.test_mq_pub_called && env.test_mq_sub_called == env.N_session*env.N_per_session_pkt_cnt);

}

struct test_overlimit_session_mq_ctx
{
	int pkt_cnt;
	int pub_cnt;
	int sub_cnt;
};

static void *test_overlimit_pub_session_ctx_new(struct session *sess, void *plugin_env)
{
	struct test_overlimit_session_mq_ctx *ctx=CALLOC(struct test_overlimit_session_mq_ctx, 1);
	return ctx;
}

static void test_overlimit_pub_session_ctx_free(struct session *sess, void *session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	struct test_overlimit_session_mq_ctx *ctx=(struct test_overlimit_session_mq_ctx *)session_ctx;
	EXPECT_EQ(ctx->pkt_cnt, env->N_per_session_pkt_cnt);
	FREE(ctx);
	return;
}

static void *test_overlimit_sub_session_ctx_new(struct session *sess, void *plugin_env)
{
	struct test_overlimit_session_mq_ctx *ctx=CALLOC(struct test_overlimit_session_mq_ctx, 1);
	return ctx;
}

static void test_overlimit_sub_session_ctx_free(struct session *sess, void *session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	struct test_overlimit_session_mq_ctx *ctx=(struct test_overlimit_session_mq_ctx *)session_ctx;
	EXPECT_EQ(ctx->sub_cnt, (env->N_per_session_pkt_cnt*(MAX_MSG_PER_DISPATCH-1))); //minus intrinsic msg
	FREE(ctx);
	return;
}

static void test_overlimit_pub_on_session(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	struct test_overlimit_session_mq_ctx *ctx=(struct test_overlimit_session_mq_ctx *)per_session_ctx;
	EXPECT_TRUE(env!=NULL);
	EXPECT_TRUE(ctx!=NULL);
	EXPECT_EQ(sess->plug_mgr_rt->plug_mgr, env->plug_mgr);
	int *pub_msg;
    if (msg)
    {
        env->test_mq_pub_called += 1;
        ctx->pkt_cnt += 1;
		for(int i=0; i < MAX_MSG_PER_DISPATCH*2; i++)
		{
			pub_msg = CALLOC(int, 1);
        	*pub_msg = env->test_mq_pub_called;
			if(i<(MAX_MSG_PER_DISPATCH-1))// minus intrinsic msg
			{
        		EXPECT_EQ(session_mq_publish_message(sess, env->test_mq_topic_id, pub_msg), 0);
				ctx->pub_cnt+=1;
			}
			else
			{
        		EXPECT_EQ(session_mq_publish_message(sess, env->test_mq_topic_id, pub_msg), -1);
				FREE(pub_msg);
			}
		}
    }
    return;
}

static void test_overlimit_on_sub_msg(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	struct test_overlimit_session_mq_ctx *ctx=(struct test_overlimit_session_mq_ctx *)per_session_ctx;
	EXPECT_TRUE(env!=NULL);
	EXPECT_TRUE(ctx!=NULL);
	EXPECT_EQ(sess->plug_mgr_rt->plug_mgr, env->plug_mgr);
	EXPECT_EQ(*(int *)msg, env->test_mq_pub_called);
	env->test_mq_sub_called+=1;
	ctx->sub_cnt+=1;
	return;
}

static void test_overlimit_session_msg_free(struct session *sess, void *msg, void *msg_free_arg)
{
	struct session_plugin_env *env = (struct session_plugin_env *)msg_free_arg;
	EXPECT_EQ(sess->plug_mgr_rt->plug_mgr, env->plug_mgr);
	if(msg)
	{
		EXPECT_EQ(env->test_mq_pub_called, *(int *)msg);
		env->test_mq_free_called+=1;
		FREE(msg);
	}
	return;
}

TEST(plugin_manager, session_plugin_pub_msg_overlimt) {

	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

	unsigned char ip_proto=6;
	struct session_plugin_env env;
	memset(&env, 0, sizeof(struct session_plugin_env));
	env.plug_mgr=plug_mgr;
	env.N_per_session_pkt_cnt=10;
	env.N_session=10;

	env.test_mq_pub_plugin_id=stellar_session_plugin_register(&st, test_overlimit_pub_session_ctx_new, test_overlimit_pub_session_ctx_free, &env);
	EXPECT_GE(env.test_mq_pub_plugin_id, 0);

	env.intrinsc_tcp_topic_id=stellar_session_mq_get_topic_id(&st, TOPIC_TCP);
	EXPECT_GE(env.intrinsc_tcp_topic_id, 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.intrinsc_tcp_topic_id, test_overlimit_pub_on_session, env.test_mq_pub_plugin_id), 0);

	env.test_mq_topic_id=stellar_session_mq_create_topic(&st, "SESSION_MQ_TOPIC", test_overlimit_session_msg_free, &env);
	EXPECT_GE(env.test_mq_topic_id, 0);

	env.test_mq_sub_plugin_id=stellar_session_plugin_register(&st, test_overlimit_sub_session_ctx_new, test_overlimit_sub_session_ctx_free, &env);
	EXPECT_GE(env.test_mq_sub_plugin_id, 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.test_mq_topic_id, test_overlimit_on_sub_msg, env.test_mq_sub_plugin_id), 0);

	struct packet pkt={&st, TCP, ip_proto};


	struct session sess[env.N_session];

	for(int i=0; i < env.N_session; i++)
		sess[i].plug_mgr_rt=plugin_manager_session_runtime_new(plug_mgr, &sess[i]);

    for (int j = 0; j < env.N_per_session_pkt_cnt; j++)
    {
        plugin_manager_on_packet_ingress(plug_mgr, &pkt);

        for (int i = 0; i < env.N_session; i++)
        {
            plugin_manager_on_session_ingress(&sess[i], &pkt);
            plugin_manager_on_session_egress(&sess[i], &pkt);
        }

    }

    for(int i=0; i < env.N_session; i++)
	{
		plugin_manager_on_session_closing(&sess[i]);
		plugin_manager_session_runtime_free(sess[i].plug_mgr_rt);
	}

	plugin_manager_exit(plug_mgr);

	EXPECT_EQ(env.test_mq_pub_called,env.N_per_session_pkt_cnt*env.N_session);
	EXPECT_EQ(env.test_mq_free_called, env.N_session*env.N_per_session_pkt_cnt*(MAX_MSG_PER_DISPATCH-1));
	EXPECT_EQ(env.test_mq_sub_called, env.N_session*env.N_per_session_pkt_cnt*(MAX_MSG_PER_DISPATCH-1));

}

static void *test_dettach_session_ctx_new(struct session *sess, void *plugin_env)
{
	struct test_basic_ctx *ctx=CALLOC(struct test_basic_ctx, 1);
	stellar_session_plugin_dettach_current_session(sess);
	ctx->called+=1;
	return ctx;
}

static void test_dettach_session_ctx_free(struct session *sess, void *session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	env->basic_ctx_free_called+=1;
	struct test_basic_ctx *ctx=(struct test_basic_ctx *)session_ctx;
	EXPECT_EQ(sess->sess_pkt_cnt, 1);// first packet ingress, call ctx_free immediately
	EXPECT_EQ(ctx->called, 1);
	FREE(ctx);
}

static void test_dettach_on_session(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct test_basic_ctx *ctx=(struct test_basic_ctx *)per_session_ctx;
	ctx->called+=1;
}

TEST(plugin_manager, session_plugin_on_ctx_new_then_dettach) {

	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

	unsigned char ip_proto=6;
	struct session_plugin_env env;
	memset(&env, 0, sizeof(struct session_plugin_env));
	env.plug_mgr=plug_mgr;
	env.N_per_session_pkt_cnt=10;
	env.N_session=10;


	int plugin_id=stellar_session_plugin_register(&st, test_dettach_session_ctx_new, test_dettach_session_ctx_free, &env);
	EXPECT_GE(plugin_id,0);

	env.intrinsc_tcp_topic_id=stellar_session_mq_get_topic_id(&st, TOPIC_TCP);
	EXPECT_GE(env.intrinsc_tcp_topic_id, 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.intrinsc_tcp_topic_id, test_dettach_on_session, plugin_id), 0);

	struct packet pkt={&st, TCP, ip_proto};


	struct session sess[env.N_session];
	memset(&sess, 0, sizeof(sess));

	for(int i=0; i < env.N_session; i++)
		sess[i].plug_mgr_rt=plugin_manager_session_runtime_new(plug_mgr, &sess[i]);

    for (int j = 0; j < env.N_per_session_pkt_cnt; j++)
    {
        plugin_manager_on_packet_ingress(plug_mgr, &pkt);

        for (int i = 0; i < env.N_session; i++)
        {
			sess[i].sess_pkt_cnt+=1;
            plugin_manager_on_session_ingress(&sess[i], &pkt);
            plugin_manager_on_session_egress(&sess[i], &pkt);
        }

    }

    for(int i=0; i < env.N_session; i++)
	{
		plugin_manager_on_session_closing(&sess[i]);
		plugin_manager_session_runtime_free(sess[i].plug_mgr_rt);
	}

	plugin_manager_exit(plug_mgr);

	EXPECT_EQ(env.basic_ctx_free_called,env.N_session);
}



static void *test_invalid_pub_msg_session_ctx_new(struct session *sess, void *plugin_env)
{
	struct test_basic_ctx *ctx=CALLOC(struct test_basic_ctx, 1);
	return ctx;
}

static void test_invalid_pub_msg_session_ctx_free(struct session *sess, void *session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	env->basic_ctx_free_called+=1;
	struct test_basic_ctx *ctx=(struct test_basic_ctx *)session_ctx;
	EXPECT_EQ(ctx->called,(env->N_per_session_pkt_cnt+1));

	EXPECT_EQ(session_mq_publish_message(sess, env->test_mq_topic_id, ctx), -1);// illegal publish when session closing

	FREE(ctx);
}

static void test_invalid_pub_msg_on_session(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct test_basic_ctx *ctx=(struct test_basic_ctx *)per_session_ctx;
	ctx->called+=1;
}

TEST(plugin_manager, session_plugin_pub_on_ctx_free) {

	struct stellar st={0};
	struct session_plugin_env env;

// pesudo init stage
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

// plugin manager register plugin

	int plugin_id=stellar_session_plugin_register(&st, test_invalid_pub_msg_session_ctx_new, test_invalid_pub_msg_session_ctx_free, &env);
	EXPECT_GE(plugin_id,0);

	env.intrinsc_tcp_topic_id=stellar_session_mq_get_topic_id(&st, TOPIC_TCP);
	EXPECT_GE(env.intrinsc_tcp_topic_id, 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.intrinsc_tcp_topic_id, test_invalid_pub_msg_on_session, plugin_id), 0);

	env.test_mq_topic_id=stellar_session_mq_create_topic(&st, "SESSION_MQ_TOPIC", NULL, &env);
	EXPECT_GE(env.test_mq_topic_id, 0);

// pesudo packet and session

	memset(&env, 0, sizeof(struct session_plugin_env));
	env.plug_mgr=plug_mgr;
	env.N_per_session_pkt_cnt=10;
	env.N_session=10;

	struct packet pkt={&st, TCP, 6};

	struct session sess[env.N_session];
	memset(&sess, 0, sizeof(sess));

// pesudo running stage
	for(int i=0; i < env.N_session; i++)
		sess[i].plug_mgr_rt=plugin_manager_session_runtime_new(plug_mgr, &sess[i]);

    for (int j = 0; j < env.N_per_session_pkt_cnt; j++)
    {
        plugin_manager_on_packet_ingress(plug_mgr, &pkt);

        for (int i = 0; i < env.N_session; i++)
        {
			sess[i].sess_pkt_cnt+=1;
            plugin_manager_on_session_ingress(&sess[i], &pkt);
            plugin_manager_on_session_egress(&sess[i], &pkt);
        }

    }

    for(int i=0; i < env.N_session; i++)
	{
		plugin_manager_on_session_closing(&sess[i]);
		plugin_manager_session_runtime_free(sess[i].plug_mgr_rt);
	}

// pesudo exit stage
	plugin_manager_exit(plug_mgr);

	EXPECT_EQ(env.basic_ctx_free_called,env.N_session);
}


struct test_session_closing_ctx
{
	int pkt_called;
	int session_free_called;
	int userdefine_on_msg_called;
};


static void *test_session_closing_ctx_new(struct session *sess, void *plugin_env)
{
	struct test_session_closing_ctx *ctx=CALLOC(struct test_session_closing_ctx, 1);
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	env->basic_ctx_new_called+=1;
	return ctx;
}

static void test_session_closing_ctx_free(struct session *sess, void *session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	env->basic_ctx_free_called+=1;
	struct test_session_closing_ctx *ctx=(struct test_session_closing_ctx *)session_ctx;
	EXPECT_EQ(ctx->pkt_called,env->N_per_session_pkt_cnt);
	EXPECT_EQ(ctx->session_free_called,1);
	FREE(ctx);
}

static void test_session_closing_on_intrisic_msg(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct test_session_closing_ctx *ctx=(struct test_session_closing_ctx *)per_session_ctx;
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	if(msg)ctx->pkt_called+=1;
	if(session_get_current_state(sess)==SESSION_STATE_CLOSING)
	{
		EXPECT_EQ(msg,nullptr);
		ctx->session_free_called+=1;
		session_mq_publish_message(sess, env->test_mq_topic_id, env);
		env->test_mq_pub_called+=1;
	}
}

static void test_session_closing_on_userdefine_msg(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct test_session_closing_ctx *ctx=(struct test_session_closing_ctx *)per_session_ctx;
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	ctx->userdefine_on_msg_called+=1;
	EXPECT_EQ(msg, plugin_env);
	EXPECT_EQ(session_get_current_state(sess),SESSION_STATE_CLOSING);
	env->test_mq_sub_called+=1;
}

TEST(plugin_manager, session_plugin_pub_msg_on_closing) {

	struct stellar st={0};
	struct session_plugin_env env;
	memset(&env, 0, sizeof(struct session_plugin_env));

// pesudo init stage
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

// plugin manager register plugin

	int plugin_id=stellar_session_plugin_register(&st, test_session_closing_ctx_new, test_session_closing_ctx_free, &env);
	EXPECT_GE(plugin_id,0);

	env.intrinsc_tcp_topic_id=stellar_session_mq_get_topic_id(&st, TOPIC_TCP);
	EXPECT_GE(env.intrinsc_tcp_topic_id, 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.intrinsc_tcp_topic_id, test_session_closing_on_intrisic_msg, plugin_id), 0);

	env.test_mq_topic_id=stellar_session_mq_create_topic(&st, "SESSION_CLOSING_TOPIC", NULL, &env);
	EXPECT_GE(env.test_mq_topic_id, 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.test_mq_topic_id, test_session_closing_on_userdefine_msg, plugin_id), 0);

// pesudo packet and session

	env.plug_mgr=plug_mgr;
	env.N_per_session_pkt_cnt=10;
	env.N_session=10;

	struct packet pkt={&st, TCP, 6};

	struct session sess[env.N_session];
	memset(&sess, 0, sizeof(sess));

// pesudo running stage
	for(int i=0; i < env.N_session; i++)
	{
		sess[i].state=SESSION_STATE_OPENING;
		sess[i].plug_mgr_rt=plugin_manager_session_runtime_new(plug_mgr, &sess[i]);
	}

    for (int j = 0; j < env.N_per_session_pkt_cnt; j++)
    {
        plugin_manager_on_packet_ingress(plug_mgr, &pkt);

        for (int i = 0; i < env.N_session; i++)
        {
			sess[i].sess_pkt_cnt+=1;
			sess[i].state=SESSION_STATE_ACTIVE;
            plugin_manager_on_session_ingress(&sess[i], &pkt);
            plugin_manager_on_session_egress(&sess[i], &pkt);
        }

    }

    for(int i=0; i < env.N_session; i++)
	{
		sess[i].state=SESSION_STATE_CLOSING;
		plugin_manager_on_session_closing(&sess[i]);
		plugin_manager_session_runtime_free(sess[i].plug_mgr_rt);
	}

// pesudo exit stage
	plugin_manager_exit(plug_mgr);

	EXPECT_EQ(env.basic_ctx_new_called,env.N_session);
	EXPECT_EQ(env.basic_ctx_free_called,env.N_session);
	EXPECT_EQ(env.test_mq_pub_called,env.N_session);
	EXPECT_EQ(env.test_mq_sub_called,env.N_session);
}

struct test_session_called_ctx
{
	int called;
};

static void *test_session_called_ctx_new(struct session *sess, void *plugin_env)
{
	struct test_session_called_ctx *ctx=CALLOC(struct test_session_called_ctx, 1);
	return ctx;
}

static void test_session_called_ctx_free(struct session *sess, void *session_ctx, void *plugin_env)
{
	FREE(session_ctx);
}

//test session topic is active
static void test_session_mq_topic_is_active_plugin_1_on_msg(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;	
	struct test_session_called_ctx *ctx=(struct test_session_called_ctx *)per_session_ctx;
	env->plugin_id_1_called+=1;
	ctx->called+=1;
	EXPECT_EQ(env->plugin_id_1, sess->plug_mgr_rt->current_session_plugin_id);
	EXPECT_EQ(env->intrinsc_tcp_topic_id, topic_id);

	session_mq_ignore_message(sess, topic_id, env->plugin_id_1);
	EXPECT_EQ(session_mq_topic_is_active(sess, topic_id), 1);
	return;
}

static void test_session_mq_topic_is_active_plugin_2_on_msg(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;	
	struct test_session_called_ctx *ctx=(struct test_session_called_ctx *)per_session_ctx;
	env->plugin_id_2_called+=1;
	ctx->called+=1;
	EXPECT_EQ(env->plugin_id_2, sess->plug_mgr_rt->current_session_plugin_id);	
	EXPECT_EQ(env->intrinsc_tcp_topic_id, topic_id);

	EXPECT_EQ(session_mq_topic_is_active(sess, topic_id), 1);

	if(ctx->called > env->N_per_session_pkt_cnt/2)
	{
		session_mq_ignore_message(sess, topic_id, env->plugin_id_2);
		EXPECT_EQ(session_mq_topic_is_active(sess, topic_id), 0);	
	}
	return;
}

TEST(plugin_manager, test_session_mq_topic_is_active) {

	struct stellar st={0};
	struct session_plugin_env env;
	memset(&env, 0, sizeof(struct session_plugin_env));

// pesudo init stage
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

// plugin manager register plugin

	int plugin_id_1=stellar_session_plugin_register(&st, test_session_called_ctx_new, test_session_called_ctx_free, &env);
	EXPECT_GE(plugin_id_1,0);

	int plugin_id_2=stellar_session_plugin_register(&st, test_session_called_ctx_new, test_session_called_ctx_free, &env);
	EXPECT_GE(plugin_id_2,0);

	env.plugin_id_1=plugin_id_1;
	env.plugin_id_2=plugin_id_2;

	env.intrinsc_tcp_topic_id=stellar_session_mq_get_topic_id(&st, TOPIC_TCP);
	EXPECT_GE(env.intrinsc_tcp_topic_id, 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.intrinsc_tcp_topic_id, test_session_mq_topic_is_active_plugin_1_on_msg, plugin_id_1), 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.intrinsc_tcp_topic_id, test_session_mq_topic_is_active_plugin_2_on_msg, plugin_id_2), 0);

// pesudo packet and session

	env.plug_mgr=plug_mgr;
	env.N_per_session_pkt_cnt=10;
	env.N_session=10;

	struct packet pkt={&st, TCP, 6};

	struct session sess[env.N_session];
	memset(&sess, 0, sizeof(sess));

// pesudo running stage
	for(int i=0; i < env.N_session; i++)
	{
		sess[i].state=SESSION_STATE_OPENING;
		sess[i].plug_mgr_rt=plugin_manager_session_runtime_new(plug_mgr, &sess[i]);
	}

    for (int j = 0; j < env.N_per_session_pkt_cnt; j++)
    {
        plugin_manager_on_packet_ingress(plug_mgr, &pkt);

        for (int i = 0; i < env.N_session; i++)
        {
			sess[i].sess_pkt_cnt+=1;
			sess[i].state=SESSION_STATE_ACTIVE;
            plugin_manager_on_session_ingress(&sess[i], &pkt);
            plugin_manager_on_session_egress(&sess[i], &pkt);
        }

    }

    for(int i=0; i < env.N_session; i++)
	{
		sess[i].state=SESSION_STATE_CLOSING;
		plugin_manager_on_session_closing(&sess[i]);
		plugin_manager_session_runtime_free(sess[i].plug_mgr_rt);
	}

// pesudo exit stage
	plugin_manager_exit(plug_mgr);

	EXPECT_EQ(env.plugin_id_1_called,env.N_session*1);// per session called once, then ignore
	EXPECT_EQ(env.plugin_id_2_called,env.N_session*(env.N_per_session_pkt_cnt/2+1));// per session called one half, then ignore
}

//test dettach session
static void test_session_dettach_plugin_1_on_msg(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;	
	struct test_session_called_ctx *ctx=(struct test_session_called_ctx *)per_session_ctx;
	env->plugin_id_1_called+=1;
	ctx->called+=1;
	EXPECT_EQ(env->plugin_id_1, sess->plug_mgr_rt->current_session_plugin_id);
	EXPECT_EQ(env->intrinsc_tcp_topic_id, topic_id);

	stellar_session_plugin_dettach_current_session(sess);
	EXPECT_EQ(session_mq_topic_is_active(sess, topic_id), 1);
	return;
}

static void test_session_dettach_plugin_2_on_msg(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;	
	struct test_session_called_ctx *ctx=(struct test_session_called_ctx *)per_session_ctx;
	env->plugin_id_2_called+=1;
	ctx->called+=1;
	EXPECT_EQ(env->plugin_id_2, sess->plug_mgr_rt->current_session_plugin_id);	
	EXPECT_EQ(env->intrinsc_tcp_topic_id, topic_id);

	EXPECT_EQ(session_mq_topic_is_active(sess, topic_id), 1);

	if(ctx->called > env->N_per_session_pkt_cnt/2)
	{
		stellar_session_plugin_dettach_current_session(sess);
		EXPECT_EQ(session_mq_topic_is_active(sess, topic_id), 0);	
	}

	return;
}

TEST(plugin_manager, test_session_dettach) {

	struct stellar st={0};
	struct session_plugin_env env;
	memset(&env, 0, sizeof(struct session_plugin_env));

// pesudo init stage
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

// plugin manager register plugin

	int plugin_id_1=stellar_session_plugin_register(&st, test_session_called_ctx_new, test_session_called_ctx_free, &env);
	EXPECT_GE(plugin_id_1,0);

	int plugin_id_2=stellar_session_plugin_register(&st, test_session_called_ctx_new, test_session_called_ctx_free, &env);
	EXPECT_GE(plugin_id_2,0);

	env.plugin_id_1=plugin_id_1;
	env.plugin_id_2=plugin_id_2;

	env.intrinsc_tcp_topic_id=stellar_session_mq_get_topic_id(&st, TOPIC_TCP);
	EXPECT_GE(env.intrinsc_tcp_topic_id, 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.intrinsc_tcp_topic_id, test_session_dettach_plugin_1_on_msg, plugin_id_1), 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.intrinsc_tcp_topic_id, test_session_dettach_plugin_2_on_msg, plugin_id_2), 0);

// pesudo packet and session

	env.plug_mgr=plug_mgr;
	env.N_per_session_pkt_cnt=10;
	env.N_session=10;

	struct packet pkt={&st, TCP, 6};

	struct session sess[env.N_session];
	memset(&sess, 0, sizeof(sess));

// pesudo running stage
	for(int i=0; i < env.N_session; i++)
	{
		sess[i].state=SESSION_STATE_OPENING;
		sess[i].plug_mgr_rt=plugin_manager_session_runtime_new(plug_mgr, &sess[i]);
	}

    for (int j = 0; j < env.N_per_session_pkt_cnt; j++)
    {
        plugin_manager_on_packet_ingress(plug_mgr, &pkt);

        for (int i = 0; i < env.N_session; i++)
        {
			sess[i].sess_pkt_cnt+=1;
			sess[i].state=SESSION_STATE_ACTIVE;
            plugin_manager_on_session_ingress(&sess[i], &pkt);
            plugin_manager_on_session_egress(&sess[i], &pkt);
        }

    }

    for(int i=0; i < env.N_session; i++)
	{
		sess[i].state=SESSION_STATE_CLOSING;
		plugin_manager_on_session_closing(&sess[i]);
		plugin_manager_session_runtime_free(sess[i].plug_mgr_rt);
	}

// pesudo exit stage
	plugin_manager_exit(plug_mgr);

	EXPECT_EQ(env.plugin_id_1_called,env.N_session*1);// per session called once, then ignore
	EXPECT_EQ(env.plugin_id_2_called,env.N_session*(env.N_per_session_pkt_cnt/2+1));// per session called one half, then ignore
}


//TODO: test case for priority mq

/**********************************************
 * TEST PLUGIN MANAGER ON POLLING PLUGIN INIT *
 **********************************************/

int test_plugin_on_polling_func(void *plugin_env)
{
	return 1;
}

TEST(plugin_manager_init, polling_plugin_register) {
	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	st.plug_mgr=plug_mgr;
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

	int plugin_id = stellar_polling_plugin_register(&st, test_plugin_on_polling_func, &st);
    {
        SCOPED_TRACE("White-box test, check stellar internal schema");
		EXPECT_TRUE(plugin_id>=POLLING_PULGIN_ID_BASE);
        struct registered_polling_plugin_schema *schema = (struct registered_polling_plugin_schema *)utarray_eltptr(
            plug_mgr->registered_polling_plugin_array, (unsigned int)(plugin_id-POLLING_PULGIN_ID_BASE));
        EXPECT_EQ(schema->on_polling, (void *)test_plugin_on_polling_func);
        EXPECT_EQ(schema->plugin_env, &st);
        EXPECT_EQ(utarray_len(plug_mgr->registered_polling_plugin_array), 1);
    }

    plugin_manager_exit(plug_mgr);
}

/**********************************************
 * TEST PLUGIN MANAGER ON POLLING PLUGIN RUNTIME *
 **********************************************/

struct polling_plugin_env
{
	struct plugin_manager_schema *plug_mgr;
	int N_polling;
	int return0_polling_called;
	int return1_polling_called;
};

int return1_plugin_on_polling(void *plugin_env)
{
	struct polling_plugin_env *env = (struct polling_plugin_env *)plugin_env;
	env->return1_polling_called+=1;
	return 1;
}

int return0_plugin_on_polling(void *plugin_env)
{
	struct polling_plugin_env *env = (struct polling_plugin_env *)plugin_env;
	env->return0_polling_called+=1;
	return 0;
}

TEST(plugin_manager, basic_polling_plugins) {

// pesudo init stage
	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	st.plug_mgr=plug_mgr;
	struct polling_plugin_env env;
	memset(&env, 0, sizeof(struct polling_plugin_env));
	env.plug_mgr=plug_mgr;
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

// plugin manager register plugin
	int plugin_id = stellar_polling_plugin_register(&st, return0_plugin_on_polling, &env);
	EXPECT_TRUE(plugin_id>=0);
	plugin_id = stellar_polling_plugin_register(&st, return1_plugin_on_polling, &env);
	EXPECT_TRUE(plugin_id>=0);

// pesudo runtime stage

	env.plug_mgr=plug_mgr;
	env.N_polling=10;

	for(int i=0; i<env.N_polling; i++)
	{
		EXPECT_EQ(plugin_manager_on_polling(plug_mgr), 1);
	}

// pesudo exit stage
	plugin_manager_exit(plug_mgr);

	EXPECT_EQ(env.return0_polling_called,env.N_polling);
	EXPECT_EQ(env.return1_polling_called,env.N_polling);

}


/**********************************************
 * GTEST MAIN *
 **********************************************/

int main(int argc, char ** argv)
{
	int ret=0;
	::testing::InitGoogleTest(&argc, argv);
	ret=RUN_ALL_TESTS();
	return ret;
}