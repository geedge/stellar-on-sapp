#include "plugin_manager.h"

#include "stellar/stellar.h"

#include "stellar/session_exdata.h"
#include "stellar/session_mq.h"

#include "bitmap/bitmap.h"
#include "uthash/utarray.h"

struct stellar_message;

struct plugin_manager_schema
{
	struct stellar *st;
	UT_array *plugin_load_specs_array;
	UT_array *session_exdata_schema_array;
	UT_array *session_mq_schema_array;
	UT_array *registered_session_plugin_array;
	UT_array *registered_packet_plugin_array;
	UT_array *registered_polling_plugin_array;
	int session_mq_topic_num;
	int session_topic_subscriber_num;
	int tcp_topic_id;
	int tcp_stream_topic_id;
	int udp_topic_id;
	int egress_topic_id;
	int control_packet_topic_id;
	int max_message_dispatch;
}__attribute__((aligned(sizeof(void*))));


struct stellar_exdata
{
	void *exdata;
};



struct stellar_exdata_schema
{
	char *name;
	union 
	{
		void *free_func;
		session_exdata_free *sess_free_func;
	};
	
	void *free_arg;
	int idx;
}__attribute__((aligned(sizeof(void*))));

struct stellar_message
{
    struct
    {
        int topic_id;
    } header;
    void *body;
    struct stellar_message *next, *prev;
} __attribute__((aligned(sizeof(void *))));

typedef struct stellar_mq_subscriber 
{
	int topic_subscriber_idx;
	int plugin_idx;
	union 
	{
    	on_session_msg_cb_func *sess_msg_cb;
	};
    struct stellar_mq_subscriber *next, *prev;
}stellar_mq_subscriber __attribute__((aligned(sizeof(void*))));


struct stellar_mq_topic_schema
{
	char *topic_name;
	void *free_cb_arg;
	int topic_id;
	int subscriber_cnt;
	int is_destroyed;
	union
	{
		void *free_cb;
		session_msg_free_cb_func *sess_msg_free_cb;
	};
	struct stellar_mq_subscriber *subscribers; 
}__attribute__((aligned(sizeof(void*))));

enum plugin_ctx_state
{ INIT, ACTIVE, EXIT };

struct session_plugin_ctx_runtime
{
	enum plugin_ctx_state state;
	int session_plugin_id;
	void *plugin_ctx;
}__attribute__((aligned(sizeof(void*))));



struct plugin_manager_runtime
{
	struct plugin_manager_schema *plug_mgr;
	struct session *sess;
	struct stellar_message *pending_mq;// message list
	struct stellar_message *delivered_mq;// message list
	struct bitmap *session_mq_status; //N * M bits, N topic, M subscriber
	struct bitmap *session_topic_status; //N bits, N topic
	struct stellar_exdata *sess_exdata_array;
	struct session_plugin_ctx_runtime *plugin_ctx_array;//N plugins TODO: call alloc and free
	int current_session_plugin_id;
	int pub_session_msg_cnt;
}__attribute__((aligned(sizeof(void*))));

struct registered_packet_plugin_schema
{
	char ip_protocol;
	plugin_on_packet_func *on_packet;
	void *plugin_env;
	UT_array *registed_packet_mq_subscriber_info;
}__attribute__((aligned(sizeof(void*))));

struct registered_polling_plugin_schema
{
	plugin_on_polling_func *on_polling;
	void *plugin_env;
}__attribute__((aligned(sizeof(void*))));

struct stellar_mq_subscriber_info
{
	int topic_id;
	int subscriber_idx;
}__attribute__((aligned(sizeof(void*))));

struct registered_session_plugin_schema
{
	session_ctx_new_func *on_ctx_new;
	session_ctx_free_func *on_ctx_free;
	void *plugin_env;
	UT_array *registed_session_mq_subscriber_info;
}__attribute__((aligned(sizeof(void*))));

#define SESSION_PULGIN_ID_BASE 0x00000
#define PACKET_PULGIN_ID_BASE 0x10000
#define POLLING_PULGIN_ID_BASE 0x20000

/*******************************
 * PLUGIN MANAGER INIT & EXIT *
 *******************************/

#define MAX_MSG_PER_DISPATCH 128

#include <dlfcn.h>

struct plugin_specific
{
	char plugin_name[256];
	plugin_on_load_func *load_cb;
	plugin_on_unload_func *unload_cb;
	void *plugin_ctx;
}__attribute__((aligned(sizeof(void*))));