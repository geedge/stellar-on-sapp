#include <gtest/gtest.h>
#include <limits.h>

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
	EXPECT_TRUE(plug_mgr->packet_exdata_schema_array==NULL);
	EXPECT_TRUE(plug_mgr->packet_mq_schema_array==NULL);

	//session exdata schema null
	EXPECT_TRUE(plug_mgr->session_exdata_schema_array==NULL);

	//session mq schema not null
	EXPECT_TRUE(plug_mgr->session_mq_schema_array!=NULL);

	//registered plugin array null
	EXPECT_TRUE(plug_mgr->registered_polling_plugin_array==NULL);
	EXPECT_TRUE(plug_mgr->registered_packet_plugin_array==NULL);
	EXPECT_TRUE(plug_mgr->registered_session_plugin_array==NULL);

	EXPECT_EQ(plug_mgr->packet_mq_topic_num, 0);

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

	EXPECT_TRUE(plug_mgr->per_thread_data!=NULL);
	int thread_num=stellar_get_worker_thread_num(st);
	for(int i=0; i<thread_num; i++)
	{
		EXPECT_TRUE(plug_mgr->per_thread_data[i].per_thread_pkt_exdata_array.exdata_array==NULL);
		EXPECT_TRUE(plug_mgr->per_thread_data[i].per_thread_pkt_mq_array.mq==NULL);
	}

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

TEST(plugin_manager_init, init_without_toml) {

	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);
	plugin_manager_exit(plug_mgr);
}

/******************************************
 * TEST PLUGIN MANAGER PACKET PLUGIN INIT *
 ******************************************/

static void test_mock_packet_exdata_free(struct packet *pkt, int idx, void *ex_ptr, void *arg){}

static void test_mock_overwrite_packet_exdata_free(struct packet *pkt, int idx, void *ex_ptr, void *arg){}

TEST(plugin_manager_init, packet_exdata_new_index_overwrite) {
	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

	const char *exdata_name="PACKET_EXDATA";
	int exdata_idx=stellar_packet_exdata_new_index(&st,exdata_name, test_mock_packet_exdata_free, &st);
	EXPECT_GE(exdata_idx, 0);
	int overwrite_idx=stellar_packet_exdata_new_index(&st,exdata_name, test_mock_overwrite_packet_exdata_free, plug_mgr);
	EXPECT_GE(overwrite_idx, 0);
	EXPECT_EQ(overwrite_idx, exdata_idx);

    {
        SCOPED_TRACE("White-box test, check stellar internal schema");
        struct stellar_exdata_schema *exdata_schema = (struct stellar_exdata_schema *)utarray_eltptr(
            plug_mgr->packet_exdata_schema_array, (unsigned int)exdata_idx);
        EXPECT_EQ(exdata_schema->free_func, (void *)test_mock_overwrite_packet_exdata_free);
        EXPECT_EQ(exdata_schema->free_arg, plug_mgr);
        EXPECT_EQ(exdata_schema->idx, exdata_idx);
        EXPECT_STREQ(exdata_schema->name, exdata_name);

        int exdata_num = utarray_len(plug_mgr->packet_exdata_schema_array);
        EXPECT_EQ(exdata_num, 1);
    }

    plugin_manager_exit(plug_mgr);
}

void test_mock_packet_msg_free(struct packet *pkt, void *msg, void *msg_free_arg){}
void test_mock_overwrite_packet_msg_free(struct packet *pkt, void *msg, void *msg_free_arg){}

TEST(plugin_manager_init, packet_mq_topic_create_and_update) {
	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

	const char *topic_name="PACKET_TOPIC";

    EXPECT_EQ(stellar_packet_mq_get_topic_id(&st, topic_name), -1); // illegal topic_name

    int topic_id = stellar_packet_mq_create_topic(&st, topic_name, test_mock_packet_msg_free, &st);
    EXPECT_GE(topic_id, 0);
    struct stellar_mq_topic_schema *topic_schema = NULL;
    {
        SCOPED_TRACE("White-box test, check stellar internal schema");
        topic_schema =
            (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr->packet_mq_schema_array, (unsigned int)topic_id);
        EXPECT_EQ(topic_schema->free_cb, (void *)test_mock_packet_msg_free);
        EXPECT_EQ(topic_schema->free_cb_arg, &st);
        EXPECT_EQ(topic_schema->topic_id, topic_id);
        EXPECT_STREQ(topic_schema->topic_name, topic_name);
    }

    EXPECT_EQ(stellar_packet_mq_get_topic_id(&st, topic_name), topic_id);
    EXPECT_EQ(stellar_packet_mq_create_topic(&st, topic_name, test_mock_overwrite_packet_msg_free, plug_mgr),
              -1); // duplicate create, return error
    {
        SCOPED_TRACE("White-box test, check stellar internal schema");
        topic_schema =
            (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr->packet_mq_schema_array, (unsigned int)topic_id);
        EXPECT_EQ(topic_schema->free_cb, (void *)test_mock_packet_msg_free);
        EXPECT_EQ(topic_schema->free_cb_arg, &st);
        EXPECT_EQ(topic_schema->topic_id, topic_id);
        EXPECT_STREQ(topic_schema->topic_name, topic_name);
    }

    EXPECT_EQ(stellar_packet_mq_update_topic(&st, topic_id, test_mock_overwrite_packet_msg_free, plug_mgr), 0);

    {
        SCOPED_TRACE("White-box test, check stellar internal schema");
        topic_schema =
            (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr->packet_mq_schema_array, (unsigned int)topic_id);
        EXPECT_EQ(topic_schema->free_cb, (void *)test_mock_overwrite_packet_msg_free);
        EXPECT_EQ(topic_schema->free_cb_arg, plug_mgr);
        EXPECT_EQ(topic_schema->topic_id, topic_id);
        EXPECT_STREQ(topic_schema->topic_name, topic_name);
        EXPECT_EQ(utarray_len(plug_mgr->packet_mq_schema_array), 1);
    }

    EXPECT_EQ(stellar_packet_mq_destroy_topic(&st, 10), -1); // illgeal topic_id

    EXPECT_EQ(stellar_packet_mq_destroy_topic(&st, topic_id), 1);
    EXPECT_EQ(stellar_packet_mq_destroy_topic(&st, topic_id), 0); // duplicate destroy, return 0;

    {
        SCOPED_TRACE("White-box test, check stellar internal schema");
        EXPECT_EQ(utarray_len(plug_mgr->packet_mq_schema_array), 1); // destory won't delete the topic schema
        EXPECT_EQ(plug_mgr->packet_mq_topic_num, 0);
    }
    plugin_manager_exit(plug_mgr);
}

void test_mock_on_packet_msg(struct packet *pkt, int topic_id, const void *msg, void *plugin_env){}

void test_mock_overwrite_on_packet_msg(struct packet *pkt, int topic_id, const void *msg, void *plugin_env){}

TEST(plugin_manager_init, packet_mq_subscribe) {

	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);


	const char *topic_name="PACKET_TOPIC";

	int topic_id=stellar_packet_mq_create_topic(&st, topic_name, test_mock_packet_msg_free, &st);
	EXPECT_GE(topic_id, 0);

	EXPECT_EQ(stellar_packet_mq_subscribe(&st, topic_id, test_mock_on_packet_msg, 10+PACKET_PULGIN_ID_BASE),-1);//illgeal plugin_id
	EXPECT_EQ(stellar_packet_mq_subscribe(&st, 10, test_mock_on_packet_msg, 10+PACKET_PULGIN_ID_BASE),-1);//illgeal topic_id & plugin_id

	int plugin_id=stellar_packet_plugin_register(&st, 6, NULL, &st);
	EXPECT_GE(plugin_id, PACKET_PULGIN_ID_BASE);

	EXPECT_EQ(stellar_packet_mq_subscribe(&st, topic_id, test_mock_on_packet_msg, plugin_id),0);
	EXPECT_EQ(stellar_packet_mq_subscribe(&st, topic_id, test_mock_overwrite_on_packet_msg, plugin_id),0);//duplicate subscribe, return 0, won't overwrite
	struct stellar_mq_topic_schema *topic_schema;
    {
        SCOPED_TRACE("White-box test, check stellar internal schema");
        topic_schema = (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr->packet_mq_schema_array, (unsigned int)topic_id);
        EXPECT_EQ(topic_schema->free_cb, (void *)test_mock_packet_msg_free);
        EXPECT_EQ(topic_schema->free_cb_arg, &st);
        EXPECT_EQ(topic_schema->topic_id, topic_id);
        EXPECT_STREQ(topic_schema->topic_name, topic_name);
    }

    EXPECT_EQ(topic_schema->subscriber_cnt, 1);
	EXPECT_EQ(topic_schema->subscribers->pkt_msg_cb, (void *)test_mock_overwrite_on_packet_msg);

	plugin_manager_exit(plug_mgr);
}

/*******************************************
 * TEST PLUGIN MANAGER PACKET PLUGIN RUNTIME*
 *******************************************/

#define PACKET_PROTO_PLUGIN_NUM 128
#define PACKET_EXDATA_NUM 2
#define PACKET_TOPIC_NUM 2
#define PACKET_MQ_SUB_NUM 2
struct packet_plugin_env
{
	struct plugin_manager_schema *plug_mgr;
	int basic_on_packet_called;
	int proto_filter_plugin_id[PACKET_PROTO_PLUGIN_NUM];
	int proto_filter_plugin_called[PACKET_PROTO_PLUGIN_NUM];
	int exdata_set_on_packet_called;
	int exdata_get_on_packet_called;
	unsigned int packet_exdata_idx[PACKET_EXDATA_NUM];
	int exdata_free_called[PACKET_EXDATA_NUM];
	unsigned int packet_topic_id[PACKET_TOPIC_NUM];
	unsigned int packet_mq_sub_plugin_id[PACKET_MQ_SUB_NUM];
	int msg_pub_cnt;
	int msg_sub_cnt;
	int msg_free_cnt;
};

static void test_basic_on_packet(struct packet *pkt, unsigned char ip_protocol, void *plugin_env)
{
	struct packet_plugin_env *env = (struct packet_plugin_env *)plugin_env;
	EXPECT_TRUE(env!=NULL);
	EXPECT_EQ(pkt->ip_proto, ip_protocol);
	EXPECT_EQ(pkt->st, env->plug_mgr->st);
	EXPECT_EQ(packet_exdata_set(pkt, 2, pkt), -1);// illegal set
	EXPECT_EQ(packet_exdata_get(pkt, 2), nullptr);// illegal get
	env->basic_on_packet_called+=1;
	return;
}

TEST(plugin_manager, basic_packet_plugin) {

	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

	unsigned char ip_proto=6;
	struct packet_plugin_env env;
	memset(&env, 0, sizeof(struct packet_plugin_env));
	env.plug_mgr=plug_mgr;
	int plugin_id=stellar_packet_plugin_register(&st, ip_proto, test_basic_on_packet, &env);
	EXPECT_GE(plugin_id, PACKET_PULGIN_ID_BASE);

    {
        SCOPED_TRACE("White-box test, check stellar internal schema");
        int packet_plugin_num = utarray_len(plug_mgr->registered_packet_plugin_array);
        EXPECT_EQ(packet_plugin_num, 1);
    }

    struct packet pkt={&st, IPv4, ip_proto};
	plugin_manager_on_packet_ingress(plug_mgr, &pkt);
	plugin_manager_on_packet_egress(plug_mgr, &pkt);
	
	plugin_manager_exit(plug_mgr);

	EXPECT_EQ(env.basic_on_packet_called, 1);
}

static void test_proto_filter_on_packet(struct packet *pkt, unsigned char ip_protocol, void *plugin_env)
{
	struct packet_plugin_env *env = (struct packet_plugin_env *)plugin_env;
	EXPECT_TRUE(env!=NULL);
	EXPECT_EQ(pkt->ip_proto, ip_protocol);
	EXPECT_EQ(pkt->st, env->plug_mgr->st);
	EXPECT_EQ(packet_exdata_set(pkt, 2, pkt), -1);// illegal set
	EXPECT_EQ(packet_exdata_get(pkt, 2), nullptr);// illegal get
	env->proto_filter_plugin_called[ip_protocol]+=1;
	return;
}

TEST(plugin_manager, packet_plugin_proto_filter) {

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
            plugin_manager_on_packet_egress(plug_mgr, &pkt);
        }
    }
    plugin_manager_exit(plug_mgr);

	for (int i = 0; i < proto_filter_plugin_num; i++)
	{
		EXPECT_EQ(env.proto_filter_plugin_called[i], N_packet);
	}
}

static void test_exdata_set_on_packet(struct packet *pkt, unsigned char ip_protocol, void *plugin_env)
{
	struct packet_plugin_env *env = (struct packet_plugin_env *)plugin_env;
	EXPECT_TRUE(env!=NULL);
	EXPECT_EQ(pkt->ip_proto, ip_protocol);
	EXPECT_EQ(pkt->st, env->plug_mgr->st);
	env->exdata_set_on_packet_called+=1;

	int exdata_idx_len=(int)(sizeof(env->packet_exdata_idx) / sizeof(env->packet_exdata_idx[0]));

	for(int i=0; i<exdata_idx_len; i++)
	{
		long long *exdata_val=CALLOC(long long , 1);
		*exdata_val=i;
		EXPECT_EQ(packet_exdata_set(pkt, env->packet_exdata_idx[i], exdata_val), 0);
	}
	return;
}

static void test_exdata_get_on_packet(struct packet *pkt, unsigned char ip_protocol, void *plugin_env)
{
	struct packet_plugin_env *env = (struct packet_plugin_env *)plugin_env;
	EXPECT_TRUE(env!=NULL);
	EXPECT_EQ(pkt->ip_proto, ip_protocol);
	EXPECT_EQ(pkt->st, env->plug_mgr->st);

	int exdata_idx_len=(int)(sizeof(env->packet_exdata_idx) / sizeof(env->packet_exdata_idx[0]));
	
	for(int i=0; i<exdata_idx_len; i++)
	{
		long long *exdata_val=(long long *)packet_exdata_get(pkt, env->packet_exdata_idx[i]);
		EXPECT_EQ(*exdata_val, i);
	}
	env->exdata_get_on_packet_called+=1;
	return;
}

static void test_packet_exdata_free(struct packet *pkt, int idx, void *ex_ptr, void *arg)
{
	struct packet_plugin_env *env = (struct packet_plugin_env *)arg;
	EXPECT_EQ(env->packet_exdata_idx[idx], idx);
	EXPECT_EQ(*(long long *)ex_ptr, idx);
	FREE(ex_ptr);
	env->exdata_free_called[idx]+=1;
	return;
}


TEST(plugin_manager, packet_exdata) {

	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

	unsigned char ip_proto=6;
	struct packet_plugin_env env;
	memset(&env, 0, sizeof(struct packet_plugin_env));
	env.plug_mgr=plug_mgr;

	char exdata_name[PACKET_EXDATA_NUM][NAME_MAX];
	int exdata_idx_len=(int)(sizeof(env.packet_exdata_idx) / sizeof(env.packet_exdata_idx[0]));
	for(int i=0; i<exdata_idx_len; i++)
	{
		sprintf(exdata_name[i], "PACKET_EXDATA_%d", i);
		env.packet_exdata_idx[i]=stellar_packet_exdata_new_index(&st, exdata_name[i], test_packet_exdata_free, &env);
        {
            SCOPED_TRACE("White-box test, check stellar internal schema");
            struct stellar_exdata_schema *exdata_schema = (struct stellar_exdata_schema *)utarray_eltptr(
                plug_mgr->packet_exdata_schema_array, env.packet_exdata_idx[i]);

            EXPECT_EQ(exdata_schema->free_func, (void *)test_packet_exdata_free);
            EXPECT_EQ(exdata_schema->free_arg, &env);
            EXPECT_EQ(exdata_schema->idx, env.packet_exdata_idx[i]);
            EXPECT_STREQ(exdata_schema->name, exdata_name[i]);
        }
    }

    {
        SCOPED_TRACE("White-box test, check stellar internal schema");
        EXPECT_EQ(utarray_len(plug_mgr->packet_exdata_schema_array), exdata_idx_len);
    }

    int exdata_set_plugin_id=stellar_packet_plugin_register(&st, ip_proto, test_exdata_set_on_packet, &env);
	EXPECT_GE(exdata_set_plugin_id, PACKET_PULGIN_ID_BASE);

	int exdata_get_plugin_id=stellar_packet_plugin_register(&st, ip_proto, test_exdata_get_on_packet, &env);
	EXPECT_GE(exdata_get_plugin_id, PACKET_PULGIN_ID_BASE);

    {
        SCOPED_TRACE("White-box test, check stellar internal schema");
        EXPECT_EQ(utarray_len(plug_mgr->registered_packet_plugin_array), 2); // Fix plugin number
    }

    struct packet pkt={&st, IPv4, ip_proto};

	int N_packet=10;

	for(int i=0; i < N_packet; i++)
	{
		plugin_manager_on_packet_ingress(plug_mgr, &pkt);
		plugin_manager_on_packet_egress(plug_mgr, &pkt);
	}
	plugin_manager_exit(plug_mgr);

	EXPECT_EQ(env.exdata_set_on_packet_called, N_packet);
	EXPECT_EQ(env.exdata_get_on_packet_called, N_packet);

	for(int i=0; i < exdata_idx_len; i++)
	{
		EXPECT_EQ(env.exdata_free_called[i], N_packet);
	}
}

static void test_packet_msg_free_cb_func(struct packet *pkt, void *msg, void *msg_free_arg)
{
	struct packet_plugin_env *env = (struct packet_plugin_env *)msg_free_arg;
	EXPECT_EQ(pkt, msg);
	env->msg_free_cnt+=1;
	return;
}

static void test_mq_on_packet_msg(struct packet *pkt, int topic_id, const void *msg, void *plugin_env)
{
	struct packet_plugin_env *env = (struct packet_plugin_env *)plugin_env;
	EXPECT_TRUE(env!=NULL);
	EXPECT_EQ(pkt->st, env->plug_mgr->st);
	env->msg_sub_cnt+=1;
	return;
}

static void test_mq_pub_on_packet(struct packet *pkt, unsigned char ip_protocol, void *plugin_env)
{
	struct packet_plugin_env *env = (struct packet_plugin_env *)plugin_env;
	EXPECT_TRUE(env!=NULL);
	EXPECT_EQ(pkt->ip_proto, ip_protocol);
	EXPECT_EQ(pkt->st, env->plug_mgr->st);
	int topic_id_num=(int)(sizeof(env->packet_topic_id) / sizeof(env->packet_topic_id[0]));
	for(int i=0; i<topic_id_num; i++)
	{
		EXPECT_EQ(packet_mq_publish_message(pkt, env->packet_topic_id[i], pkt), 0);
		env->msg_pub_cnt+=1;
	}
	return;
}

TEST(plugin_manager, packet_mq) {

	struct stellar st={0};
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

	unsigned char ip_proto=6;
	struct packet_plugin_env env;
	memset(&env, 0, sizeof(struct packet_plugin_env));
	env.plug_mgr=plug_mgr;
	char topic_name[PACKET_TOPIC_NUM][NAME_MAX];

	int topic_id_num=(int)(sizeof(env.packet_topic_id) / sizeof(env.packet_topic_id[0]));

	for(int i=0; i<topic_id_num; i++)
	{
		sprintf(topic_name[i], "PACKET_TOPIC_%d", i);
		env.packet_topic_id[i]=stellar_packet_mq_create_topic(&st, topic_name[i], test_packet_msg_free_cb_func, &env);
		EXPECT_GE(env.packet_topic_id[i], 0);
        {
            SCOPED_TRACE("White-box test, check stellar internal schema");
            struct stellar_mq_topic_schema *topic = (struct stellar_mq_topic_schema *)utarray_eltptr(
                plug_mgr->packet_mq_schema_array, env.packet_topic_id[i]);
            EXPECT_EQ(topic->free_cb, test_packet_msg_free_cb_func);
            EXPECT_EQ(topic->free_cb_arg, &env);
            EXPECT_EQ(topic->topic_id, env.packet_topic_id[i]);
            EXPECT_STREQ(topic->topic_name, topic_name[i]);
        }
    }

	{
		SCOPED_TRACE("White-box test, check stellar internal schema");
		EXPECT_EQ(utarray_len(plug_mgr->packet_mq_schema_array), topic_id_num);
	}

	int pub_plugin_id=stellar_packet_plugin_register(&st, ip_proto, test_mq_pub_on_packet, &env);
	EXPECT_GE(pub_plugin_id, PACKET_PULGIN_ID_BASE);

	int topic_sub_num=(int)(sizeof(env.packet_mq_sub_plugin_id) / sizeof(env.packet_mq_sub_plugin_id[0]));

    for (int i = 0; i < topic_sub_num; i++)
    {
        env.packet_mq_sub_plugin_id[i] = stellar_packet_plugin_register(&st, ip_proto, NULL, &env);// empty on_packet is ok
        EXPECT_GE(env.packet_mq_sub_plugin_id[i], PACKET_PULGIN_ID_BASE);
		for(int j = 0; j < topic_id_num; j++)
		{
			EXPECT_EQ(stellar_packet_mq_subscribe(&st, env.packet_topic_id[j], test_mq_on_packet_msg, env.packet_mq_sub_plugin_id[i]), 0);
		}
    }

	{
		SCOPED_TRACE("White-box test, check stellar internal schema");
		EXPECT_EQ(utarray_len(plug_mgr->registered_packet_plugin_array), topic_sub_num+1);
	}

	struct packet pkt={&st, IPv4, ip_proto};

	int N_packet=10;
    for (int i = 0; i < N_packet; i++)
    {
        plugin_manager_on_packet_ingress(plug_mgr, &pkt);
        plugin_manager_on_packet_egress(plug_mgr, &pkt);
    }

    plugin_manager_exit(plug_mgr);
	EXPECT_EQ(N_packet*topic_id_num, env.msg_pub_cnt);
	EXPECT_EQ(env.msg_free_cnt, env.msg_pub_cnt);
	EXPECT_EQ(env.msg_sub_cnt, env.msg_pub_cnt*topic_sub_num);
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

    EXPECT_EQ(stellar_packet_mq_destroy_topic(&st, 10), -1);// illgeal packet topic_id
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


TEST(plugin_manager_init, session_mq_subscribe) {

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
	EXPECT_EQ(topic_schema->subscribers->pkt_msg_cb, (void *)test_mock_overwrite_on_session_msg);

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
};

TEST(plugin_manager, no_plugin_runtime) {

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

        plugin_manager_on_packet_egress(plug_mgr, &pkt);
    }

    for(int i=0; i < env.N_session; i++)
		plugin_manager_session_runtime_free(sess[i].plug_mgr_rt);

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
	EXPECT_EQ(ctx->called, env->N_per_session_pkt_cnt*2);//ingress + egress
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
	env->basic_on_session_ingress_called+=1;
	ctx->called+=1;
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

TEST(plugin_manager, basic_session_plugin) {

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

        plugin_manager_on_packet_egress(plug_mgr, &pkt);
    }

    for(int i=0; i < env.N_session; i++)
		plugin_manager_session_runtime_free(sess[i].plug_mgr_rt);

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
	env->test_mq_pub_called+=1;
	ctx->called+=1;
	int *pub_msg=(int *)CALLOC(int, 1);
	*pub_msg=env->test_mq_pub_called;
	EXPECT_EQ(session_mq_publish_message(sess, env->test_mq_topic_id, pub_msg),0);
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
	EXPECT_EQ(env->test_mq_pub_called, *(int *)msg);
	env->test_mq_free_called+=1;
	FREE(msg);
	return;
}

TEST(plugin_manager, basic_session_plugin_mq) {

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

        plugin_manager_on_packet_egress(plug_mgr, &pkt);
    }

    for(int i=0; i < env.N_session; i++)
		plugin_manager_session_runtime_free(sess[i].plug_mgr_rt);

	plugin_manager_exit(plug_mgr);

	EXPECT_TRUE(env.test_mq_free_called == env.test_mq_pub_called && env.test_mq_sub_called == env.N_session*env.N_per_session_pkt_cnt);

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

TEST(plugin_manager, session_plugin_ctx_new_dettach) {

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

        plugin_manager_on_packet_egress(plug_mgr, &pkt);
    }

    for(int i=0; i < env.N_session; i++)
		plugin_manager_session_runtime_free(sess[i].plug_mgr_rt);

	plugin_manager_exit(plug_mgr);

	EXPECT_EQ(env.basic_ctx_free_called,env.N_session);
}



static void *test_invalid_send_msg_session_ctx_new(struct session *sess, void *plugin_env)
{
	struct test_basic_ctx *ctx=CALLOC(struct test_basic_ctx, 1);
	return ctx;
}

static void test_invalid_send_msg_session_ctx_free(struct session *sess, void *session_ctx, void *plugin_env)
{
	struct session_plugin_env *env = (struct session_plugin_env *)plugin_env;
	env->basic_ctx_free_called+=1;
	struct test_basic_ctx *ctx=(struct test_basic_ctx *)session_ctx;
	EXPECT_EQ(ctx->called,env->N_per_session_pkt_cnt);

	EXPECT_EQ(session_mq_publish_message(sess, env->test_mq_topic_id, ctx), -1);// illegal publish when session closing

	FREE(ctx);
}

static void test_invalid_send_msg_on_session(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env)
{
	struct test_basic_ctx *ctx=(struct test_basic_ctx *)per_session_ctx;
	ctx->called+=1;
}

TEST(plugin_manager, session_plugin_invalid_send_msg) {

	struct stellar st={0};
	struct session_plugin_env env;

// pesudo init stage
	struct plugin_manager_schema *plug_mgr = plugin_manager_init(&st, NULL);
	whitebox_test_plugin_manager_intrisic_metadata(&st, plug_mgr);

// plugin manager register plugin

	int plugin_id=stellar_session_plugin_register(&st, test_invalid_send_msg_session_ctx_new, test_invalid_send_msg_session_ctx_free, &env);
	EXPECT_GE(plugin_id,0);

	env.intrinsc_tcp_topic_id=stellar_session_mq_get_topic_id(&st, TOPIC_TCP);
	EXPECT_GE(env.intrinsc_tcp_topic_id, 0);
	EXPECT_EQ(stellar_session_mq_subscribe(&st, env.intrinsc_tcp_topic_id, test_invalid_send_msg_on_session, plugin_id), 0);

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

        plugin_manager_on_packet_egress(plug_mgr, &pkt);
    }

    for(int i=0; i < env.N_session; i++)
		plugin_manager_session_runtime_free(sess[i].plug_mgr_rt);

// pesudo exit stage
	plugin_manager_exit(plug_mgr);
}

/**********************************************
 * TEST PLUGIN MANAGER ON POLLING PLUGIN INIT *
 **********************************************/

/**********************************************
 * TEST PLUGIN MANAGER ON POLLING PLUGIN RUNTIME *
 **********************************************/

int main(int argc, char ** argv)
{
	int ret=0;
	::testing::InitGoogleTest(&argc, argv);
	ret=RUN_ALL_TESTS();
	return ret;
}