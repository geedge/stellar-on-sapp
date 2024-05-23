#include "plugin_manager.h"
#include "stellar_internal.h"

#include "stellar/utils.h"
#include "stellar/session.h"
#include "stellar/session_exdata.h"
#include "stellar/session_mq.h"

#include "uthash/utlist.h"
#include "uthash/utarray.h"

#include "bitmap/bitmap.h"

struct plugin_manager_schema
{
	struct stellar *st;
	UT_array *session_exdata_schema_array;
	UT_array *plugin_load_specs_array;
	UT_array *session_mq_schema_array;
	UT_array *registered_session_plugin_array;
	UT_array *registered_packet_plugin_array;
	UT_array *registered_polling_plugin_array;
	int topic_num;
	int subscriber_num;
	int tcp_topic_id;
	int udp_topic_id;
	int tcp_stream_topic_id;
	int egress_topic_id;
	int control_packet_topic_id;
};

struct session_exdata_schema
{
	char *name;
	session_exdata_free *free_func;
	void *free_arg;
	int idx;
};

struct session_message
{
	int topic_id;
    void *msg_data;
    struct session_message *next, *prev;
};

typedef struct session_mq_subscriber 
{
	int topic_subscriber_idx;
	int session_plugin_id;
    on_msg_cb_func *msg_cb;
    struct session_mq_subscriber *next, *prev;
}session_mq_subscribers;

struct session_mq_topic_schema
{
	char *topic_name;
	msg_free_cb_func *free_cb;
	void *free_cb_arg;
	int topic_id;
	int subscriber_cnt;
	struct session_mq_subscriber *subscribers; 
};

enum plugin_ctx_state
{ INIT, ACTIVE, EXIT };

struct session_plugin_ctx_runtime
{
	enum plugin_ctx_state state;
	int session_plugin_id;
	void *plugin_ctx;
};

struct plugin_exdata
{
	void *exdata;
};

struct plugin_manager_runtime
{
	struct plugin_manager_schema *plug_mgr;
	struct session *sess;
	struct session_message *pending_mq;// message list
	struct session_message *delivered_mq;// message list
	struct bitmap *session_mq_status; //N * M bits, N topic, M subscriber
	struct plugin_exdata *plugin_exdata_array;
	struct session_plugin_ctx_runtime *plugin_ctx_array;//N plugins TODO: call alloc and free
	int current_session_plugin_id;
};

struct registered_packet_plugin_schema
{
	char ip_protocol;
	plugin_on_packet_func *on_packet;
	void *plugin_env;
};

struct registered_polling_plugin_schema
{
	plugin_on_polling_func *on_polling;
	void *plugin_env;
};

struct session_mq_subscriber_info
{
	int topic_id;
	int subscriber_idx;
};

struct registered_session_plugin_schema
{
	session_ctx_new_func *on_ctx_new;
	session_ctx_free_func *on_ctx_free;
	void *plugin_env;
	UT_array *registed_session_mq_subscriber_info;
};

#define PACKET_PULGIN_ID_BASE 0x10000
#define POLLING_PULGIN_ID_BASE 0x20000

/*******************************
 * PLUGIN MANAGER INIT & EXIT *
 *******************************/

#include <dlfcn.h>
#include "toml/toml.h"

struct plugin_specific
{
	char plugin_name[256];
	plugin_on_load_func *load_cb;
	plugin_on_unload_func *unload_cb;
	void *plugin_ctx;
};

UT_icd plugin_specs_icd = {sizeof(struct plugin_specific), NULL, NULL, NULL};

static struct plugin_specific *plugin_specs_load(const char *toml_conf_path, int *spec_num)
{
	*spec_num = 0;
    FILE* fp = fopen(toml_conf_path, "r");
	if(fp==NULL)return NULL;
    char errbuf[256];
    toml_table_t* conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);	
    if (!conf) {
        fprintf(stderr, "Error parsing toml: %s\n", errbuf);
        return NULL;
    }	
    toml_array_t* plugin_array = toml_array_in(conf, "plugin");
	if(plugin_array==NULL)return NULL;
    *spec_num = toml_array_nelem(plugin_array);
     struct plugin_specific* plugins = CALLOC(struct plugin_specific, *spec_num);

    for (int i = 0; i < *spec_num; i++) {
        toml_table_t* plugin = toml_table_at(plugin_array, i);

        const char *path_raw = toml_raw_in(plugin, "path");
        const char *init_func_name_raw = toml_raw_in(plugin, "init");
        const char *exit_func_name_raw = toml_raw_in(plugin, "exit");
        char *path = NULL;
        char *init_func_name = NULL;
        char *exit_func_name = NULL;
        if (toml_rtos(path_raw, &path) || toml_rtos(init_func_name_raw, &init_func_name) ||
            toml_rtos(exit_func_name_raw, &exit_func_name))
        {
            goto PLUGIN_SPEC_LOAD_ERROR;
        }
        void* handle = dlopen(path, RTLD_NOW|RTLD_LAZY|RTLD_GLOBAL);
        if (!handle) {
            fprintf(stderr, "Error loading plugin %s: %s\n", path, dlerror());
            goto PLUGIN_SPEC_LOAD_ERROR;
        }

        plugins[i].load_cb = (plugin_on_load_func *) dlsym(handle, init_func_name);
        if (!plugins[i].load_cb) {
            fprintf(stderr, "Could not load init function %s: %s\n", init_func_name, dlerror());
        }

        plugins[i].unload_cb = (plugin_on_unload_func *) dlsym(handle, exit_func_name);
        if (!plugins[i].unload_cb) {
            fprintf(stderr, "Could not load exit function %s: %s\n", exit_func_name, dlerror());
        }
        FREE(path);
        FREE(init_func_name);
        FREE(exit_func_name);
    }
    toml_free(conf);
    return plugins;	
PLUGIN_SPEC_LOAD_ERROR:
	toml_free(conf);
	FREE(plugins);
	return NULL;	
}

struct plugin_manager_schema *plugin_manager_init(struct stellar *st, const char *plugin_spec_file_path)
{
	int spec_num;
	struct plugin_specific *specs = plugin_specs_load(plugin_spec_file_path, &spec_num);
	if(spec_num < 0)
	{
		return NULL;
	}
	struct plugin_manager_schema *pm = CALLOC(struct plugin_manager_schema, 1);
	if(spec_num > 0)
	{
		utarray_new(pm->plugin_load_specs_array,&plugin_specs_icd);
		utarray_reserve(pm->plugin_load_specs_array, spec_num);
	}

	pm->st = st;
	stellar_plugin_manager_schema_set(st, pm);


	pm->tcp_topic_id=stellar_session_mq_create_topic(st, TOPIC_TCP, NULL, NULL);
	pm->tcp_stream_topic_id=stellar_session_mq_create_topic(st, TOPIC_TCP_STREAM, NULL, NULL);
	pm->udp_topic_id=stellar_session_mq_create_topic(st, TOPIC_UDP, NULL, NULL);
	pm->egress_topic_id=stellar_session_mq_create_topic(st, TOPIC_EGRESS, NULL, NULL);
	pm->control_packet_topic_id=stellar_session_mq_create_topic(st, TOPIC_CONTROL_PACKET, NULL, NULL);

	for(int i = 0; i < spec_num; i++)
	{
		if (specs[i].load_cb != NULL)
		{
			specs[i].plugin_ctx=specs[i].load_cb(st);
			utarray_push_back(pm->plugin_load_specs_array, &specs[i]);
		}
	}
	FREE(specs);
	return pm;
}

void plugin_manager_exit(struct plugin_manager_schema *plug_mgr)
{
	struct plugin_specific *p=NULL;
    if (plug_mgr->plugin_load_specs_array)
    {
        while ((p = (struct plugin_specific *)utarray_next(plug_mgr->plugin_load_specs_array, p)))
        {
            if (p->unload_cb)
                p->unload_cb(p->plugin_ctx);
        }
        utarray_free(plug_mgr->plugin_load_specs_array);
    }
    if(plug_mgr->session_mq_schema_array)
	{
		for(unsigned int i = 0; i < utarray_len(plug_mgr->session_mq_schema_array); i++)
		{
			stellar_session_mq_destroy_topic(plug_mgr->st, i);
		}
		utarray_free(plug_mgr->session_mq_schema_array);
	}
	if(plug_mgr->session_exdata_schema_array)utarray_free(plug_mgr->session_exdata_schema_array);
	if(plug_mgr->registered_packet_plugin_array)utarray_free(plug_mgr->registered_packet_plugin_array);
	if(plug_mgr->registered_polling_plugin_array)utarray_free(plug_mgr->registered_polling_plugin_array);
	if(plug_mgr->registered_session_plugin_array)
	{
        struct registered_session_plugin_schema *s = NULL;
        while ((s = (struct registered_session_plugin_schema *)utarray_next(plug_mgr->registered_session_plugin_array, s)))
        {
			if(s->registed_session_mq_subscriber_info)utarray_free(s->registed_session_mq_subscriber_info);
        }
        utarray_free(plug_mgr->registered_session_plugin_array);
	}
	FREE(plug_mgr);
	return;
}


/*******************************
 * SESSION EXDATA *
 *******************************/

static void session_exdata_met_copy(void *_dst, const void *_src)
{
    struct session_exdata_schema *dst = (struct session_exdata_schema *)_dst, *src = (struct session_exdata_schema *)_src;
    dst->free_func = src->free_func;
    dst->free_arg = src->free_arg;
    dst->idx = src->idx;
    dst->name = src->name ? strdup(src->name) : NULL;
}

static void session_exdata_met_dtor(void *_elt)
{
    struct session_exdata_schema *elt = (struct session_exdata_schema *)_elt;
    if (elt->name)
        FREE(elt->name);
}

UT_icd session_exdata_meta_icd = {sizeof(struct session_exdata_schema), NULL, session_exdata_met_copy, session_exdata_met_dtor};


int stellar_session_exdata_new_index(struct stellar *st, const char *name, session_exdata_free *free_func,void *free_arg) 
{
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	if(plug_mgr->session_exdata_schema_array == NULL)
	{
		utarray_new(plug_mgr->session_exdata_schema_array, &session_exdata_meta_icd);
	}
	if(plug_mgr->session_exdata_schema_array == NULL)return -1;
	unsigned int len = utarray_len(plug_mgr->session_exdata_schema_array);
	struct session_exdata_schema *t_schema;
	for(unsigned int i = 0; i < len; i++)
	{
		t_schema = (struct session_exdata_schema *)utarray_eltptr(plug_mgr->session_exdata_schema_array, i);
		if(strcmp(t_schema->name, name) == 0)
		{
			return t_schema->idx;
		}
	}
	struct session_exdata_schema new_schema;
	memset(&new_schema, 0, sizeof(struct session_exdata_schema));
	new_schema.free_func=free_func;
	new_schema.name=(char *)name;
	new_schema.idx=len;
	new_schema.free_arg=free_arg;
	utarray_push_back(plug_mgr->session_exdata_schema_array, &new_schema);
	return new_schema.idx;
}

int session_exdata_set(struct session *sess, int idx, void *ex_ptr)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	if(plug_mgr_rt == NULL)return -1;
	if(plug_mgr_rt->plug_mgr->session_exdata_schema_array == NULL)return -1;
	unsigned int len=utarray_len(plug_mgr_rt->plug_mgr->session_exdata_schema_array);
	if(len < (unsigned int)idx)return -1;
	if(plug_mgr_rt->plugin_exdata_array==NULL)return -1;
	(plug_mgr_rt->plugin_exdata_array+idx)->exdata=ex_ptr;
	return 0;
}

void *session_exdata_get(struct session *sess, int idx)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	if(plug_mgr_rt == NULL)return NULL;
	if(plug_mgr_rt->plug_mgr->session_exdata_schema_array==NULL)return NULL;
	unsigned int len = utarray_len(plug_mgr_rt->plug_mgr->session_exdata_schema_array);
	if(len < (unsigned int)idx)return NULL;
	return (plug_mgr_rt->plugin_exdata_array+idx)->exdata;
}

/*******************************
 * SESSION MQ *
 *******************************/



static void session_mq_topic_schema_copy(void *_dst, const void *_src)
{
    struct session_mq_topic_schema *dst = (struct session_mq_topic_schema *)_dst,
                                   *src = (struct session_mq_topic_schema *)_src;
    dst->subscribers = src->subscribers;
    dst->free_cb = src->free_cb;
    dst->free_cb_arg = src->free_cb_arg;
    dst->topic_id = src->topic_id;
    dst->subscriber_cnt = src->subscriber_cnt;
    dst->topic_name = src->topic_name ? strdup(src->topic_name) : NULL;
}

static void session_mq_topic_schema_dtor(void *_elt)
{
    struct session_mq_topic_schema *elt = (struct session_mq_topic_schema *)_elt;
    if (elt->topic_name)
        FREE(elt->topic_name);
    // FREE(elt); // free the item
}

UT_icd session_mq_topic_schema_icd = {sizeof(struct session_mq_topic_schema), NULL, session_mq_topic_schema_copy, session_mq_topic_schema_dtor};

void session_mq_free(struct session_message *head, UT_array *session_mq_schema_array)
{
	struct session_message *mq_elt, *tmp;
	struct session_mq_topic_schema *topic;
    DL_FOREACH_SAFE(head, mq_elt, tmp)
    {
		topic = (struct session_mq_topic_schema *)utarray_eltptr(session_mq_schema_array,
                                                                    (unsigned int)(mq_elt->topic_id));
        if (topic && topic->free_cb)
        {
            topic->free_cb(mq_elt->msg_data, topic->free_cb_arg);
        }
        DL_DELETE(head, mq_elt);
        FREE(mq_elt);
    }
	FREE(head);
}

int stellar_session_mq_get_topic_id(struct stellar *st, const char *topic_name)
{
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);;
	if(topic_name == NULL || plug_mgr == NULL || plug_mgr->session_mq_schema_array == NULL)return -1;
	unsigned int len = utarray_len(plug_mgr->session_mq_schema_array);
	struct session_mq_topic_schema *t_schema;
	for(unsigned int i = 0; i < len; i++)
	{
		t_schema = (struct session_mq_topic_schema *)utarray_eltptr(plug_mgr->session_mq_schema_array, i);
		if(strcmp(t_schema->topic_name, topic_name) == 0)
		{
			return i;
		}
	}	
	return -1;
}

int stellar_session_mq_update_topic(struct stellar *st, int topic_id, msg_free_cb_func *msg_free_cb, void *msg_free_arg)
{
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	if(plug_mgr->session_mq_schema_array == NULL)return -1;
	unsigned int len = utarray_len(plug_mgr->session_mq_schema_array);
	if(len < (unsigned int)topic_id)return -1;
	struct session_mq_topic_schema *t_schema = (struct session_mq_topic_schema *)utarray_eltptr(plug_mgr->session_mq_schema_array, (unsigned int)topic_id);
	if(t_schema == NULL)return -1;
	t_schema->free_cb=msg_free_cb;
	t_schema->free_cb_arg=msg_free_arg;
	return 0;
}

int stellar_session_mq_create_topic(struct stellar *st, const char *topic_name, msg_free_cb_func *msg_free_cb, void *msg_free_arg)
{
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	if(plug_mgr->session_mq_schema_array == NULL)
	{
		utarray_new(plug_mgr->session_mq_schema_array, &session_mq_topic_schema_icd);
	}
	unsigned int len = utarray_len(plug_mgr->session_mq_schema_array);
	if(stellar_session_mq_get_topic_id(st, topic_name) >= 0)
	{
		return -1;
	}
	struct session_mq_topic_schema t_schema;
	memset(&t_schema, 0, sizeof(struct session_mq_topic_schema));
	t_schema.free_cb=msg_free_cb;
	t_schema.topic_name=(char *)topic_name;
	t_schema.topic_id=len;//topid_id equals arrary index
	t_schema.free_cb_arg=msg_free_arg;
	t_schema.subscribers=NULL;
	t_schema.subscriber_cnt=0;
	utarray_push_back(plug_mgr->session_mq_schema_array, &t_schema);
	plug_mgr->topic_num+=1;
	return t_schema.topic_id;	
}

int stellar_session_mq_destroy_topic(struct stellar *st, int topic_id)
{
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	if(plug_mgr->session_mq_schema_array==NULL)return 0;
    unsigned int len = utarray_len(plug_mgr->session_mq_schema_array);
    if (len <= (unsigned int)topic_id)
        return -1;
    struct session_mq_topic_schema *topic =
        (struct session_mq_topic_schema *)utarray_eltptr(plug_mgr->session_mq_schema_array, (unsigned int)topic_id);
    struct session_mq_subscriber *sub_elt, *sub_tmp;

    if (topic)
    {
        DL_FOREACH_SAFE(topic->subscribers, sub_elt, sub_tmp)
        {
            DL_DELETE(topic->subscribers, sub_elt);
            FREE(sub_elt);
        }
    }
    return 0; // success
}

int session_mq_publish_message(struct session *sess, int topic_id, void *data)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	if(plug_mgr_rt==NULL || topic_id < 0)return -1;
	if(plug_mgr_rt->plug_mgr->session_mq_schema_array==NULL)return -1;
    unsigned int len = utarray_len(plug_mgr_rt->plug_mgr->session_mq_schema_array);
    if (len <= (unsigned int)topic_id)return -1;
	struct session_message *msg= CALLOC(struct session_message,1);
	msg->topic_id = topic_id;
	msg->msg_data = data;
	DL_APPEND(plug_mgr_rt->pending_mq, msg);
	return 0;
}

static int session_mq_set_message_status(struct session *sess, int topic_id, int plugin_id, int bit_value)
{
	if(bit_value!=0 && bit_value!=1)return -1;
	if(plugin_id >= PACKET_PULGIN_ID_BASE)return -1;// ignore packet plugin
	if(topic_id < 0 || plugin_id < 0)return -1;
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	if(plug_mgr_rt==NULL)return -1;
	if(topic_id >= plug_mgr_rt->plug_mgr->topic_num)return -1;// topic_id out of range
	struct session_mq_topic_schema *topic = (struct session_mq_topic_schema *)utarray_eltptr(plug_mgr_rt->plug_mgr->session_mq_schema_array, (unsigned int)topic_id);
	if(topic==NULL)return -1;

	struct registered_session_plugin_schema *session_plugin_schema = (struct registered_session_plugin_schema *)utarray_eltptr(plug_mgr_rt->plug_mgr->registered_session_plugin_array, (unsigned int)plugin_id);
	if(session_plugin_schema==NULL)return -1;

	unsigned int plugin_subscriber_num = utarray_len(session_plugin_schema->registed_session_mq_subscriber_info);
	if(plug_mgr_rt->session_mq_status)
	{
		for(unsigned int i=0; i < plugin_subscriber_num; i++)
		{
			struct session_mq_subscriber_info *session_plugin_sub_info = (struct session_mq_subscriber_info *)utarray_eltptr(session_plugin_schema->registed_session_mq_subscriber_info, i);
			if(topic_id==session_plugin_sub_info->topic_id)
			{
				bitmap_set(plug_mgr_rt->session_mq_status, topic_id, session_plugin_sub_info->subscriber_idx, bit_value);
			}
		}
		return 0;
	}
	return -1;
}


int session_mq_ignore_message(struct session *sess, int topic_id, int plugin_id)
{
	return session_mq_set_message_status(sess, topic_id, plugin_id, 0);

}

int session_mq_unignore_message(struct session *sess, int topic_id, int plugin_id)
{
	return session_mq_set_message_status(sess, topic_id, plugin_id, 1);
}

UT_icd session_mq_subscriber_info_icd = {sizeof(struct session_mq_subscriber_info), NULL, NULL, NULL};

int stellar_session_mq_subscribe(struct stellar *st, int topic_id, on_msg_cb_func *plugin_on_msg_cb, int plugin_id)
{
	if(plugin_id >= PACKET_PULGIN_ID_BASE)return -1;// ignore packet plugin
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	if(plug_mgr->session_mq_schema_array==NULL)return -1;
    unsigned int len = utarray_len(plug_mgr->session_mq_schema_array);
    if (len <= (unsigned int)topic_id)return -1;

	struct registered_session_plugin_schema *session_plugin_schema = (struct registered_session_plugin_schema *)utarray_eltptr(plug_mgr->registered_session_plugin_array, (unsigned)plugin_id);
	if(session_plugin_schema==NULL)return -1;

    struct session_mq_topic_schema *topic = (struct session_mq_topic_schema *)utarray_eltptr(plug_mgr->session_mq_schema_array, (unsigned int)topic_id);
	if(topic==NULL)return -1;

	if(session_plugin_schema->registed_session_mq_subscriber_info==NULL)
	{
		utarray_new(session_plugin_schema->registed_session_mq_subscriber_info, &session_mq_subscriber_info_icd);
	}

	// if plugin already subscribe current topic, return 0
	struct session_mq_subscriber_info *p=NULL;
  	while( (p=(struct session_mq_subscriber_info *)utarray_next(session_plugin_schema->registed_session_mq_subscriber_info,p))) 
	{
		if(p->topic_id==topic_id)
		return 0;
	};

	struct session_mq_subscriber *new_subscriber = CALLOC(struct session_mq_subscriber,1);
	new_subscriber->topic_subscriber_idx = topic->subscriber_cnt;
	new_subscriber->session_plugin_id = plugin_id;
    new_subscriber->msg_cb = plugin_on_msg_cb;
	DL_APPEND(topic->subscribers, new_subscriber);

	struct session_mq_subscriber_info sub_info;
	memset(&sub_info, 0, sizeof(struct session_mq_subscriber_info));
	sub_info.topic_id=topic_id;
	sub_info.subscriber_idx=topic->subscriber_cnt;	
	utarray_push_back(session_plugin_schema->registed_session_mq_subscriber_info, &sub_info);
	topic->subscriber_cnt+=1;
	plug_mgr->subscriber_num+=1;
	return 0;
}

static void plugin_manager_session_message_dispatch(struct session *sess)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	if(plug_mgr_rt==NULL)return;

	struct session_message *mq_elt=NULL, *mq_tmp=NULL;
	struct session_mq_subscriber *sub_elt, *sub_tmp;
	struct session_mq_topic_schema *topic;
	struct registered_session_plugin_schema *session_plugin_schema;
	struct session_plugin_ctx_runtime *plugin_ctx_rt;
    while (plug_mgr_rt->pending_mq != NULL)
    {
        DL_FOREACH_SAFE(plug_mgr_rt->pending_mq, mq_elt, mq_tmp)
        {
            topic = (struct session_mq_topic_schema *)utarray_eltptr(plug_mgr_rt->plug_mgr->session_mq_schema_array,
                                                                    (unsigned int)(mq_elt->topic_id));
            if (topic)
            {
                int cur_sub_idx = 0;
                DL_FOREACH_SAFE(topic->subscribers, sub_elt, sub_tmp)
                {
                    if (bitmap_get(plug_mgr_rt->session_mq_status, mq_elt->topic_id, cur_sub_idx) != 0)
                    {
						plugin_ctx_rt=(plug_mgr_rt->plugin_ctx_array+sub_elt->session_plugin_id);
						session_plugin_schema = (struct registered_session_plugin_schema *)utarray_eltptr(plug_mgr_rt->plug_mgr->registered_session_plugin_array, (unsigned int)sub_elt->session_plugin_id);
						if(plugin_ctx_rt->state==INIT)
						{
							if(session_plugin_schema->on_ctx_new)
							{
								plugin_ctx_rt->plugin_ctx=session_plugin_schema->on_ctx_new(sess, session_plugin_schema->plugin_env);
								plugin_ctx_rt->state=ACTIVE;
							}
						}
						if(sub_elt->msg_cb)sub_elt->msg_cb(sess, mq_elt->topic_id, mq_elt->msg_data, plugin_ctx_rt->plugin_ctx,
                                        session_plugin_schema->plugin_env); 
                    }
                    cur_sub_idx++;
                }
            }
            DL_DELETE(plug_mgr_rt->pending_mq, mq_elt);
			DL_APPEND(plug_mgr_rt->delivered_mq, mq_elt);// move to delivered message list
        }
    }
	return;
}

/*******************************
 * PLUGIN MANAGER SESSION RUNTIME *
 *******************************/

static struct plugin_exdata *session_exdata_runtime_new(struct plugin_manager_schema *plug_mgr)
{
	struct plugin_exdata *exdata_rt = NULL;
	if(plug_mgr->session_exdata_schema_array==NULL)return NULL;
	unsigned int len = utarray_len(plug_mgr->session_exdata_schema_array);
	if(len > 0)
	{
		exdata_rt=CALLOC(struct plugin_exdata, len);
	}
	return exdata_rt;
}

static void session_exdata_runtime_free(struct plugin_manager_schema *plug_mgr, struct session *sess, struct plugin_exdata *exdata_rt)
{
	if(exdata_rt==NULL)return;
	if(plug_mgr->session_exdata_schema_array==NULL)return;
	unsigned int len=utarray_len(plug_mgr->session_exdata_schema_array);
	for (unsigned int i = 0; i < len; i++)
	{
		void *exdata = (exdata_rt + i)->exdata;
		struct session_exdata_schema *schema = (struct session_exdata_schema *)utarray_eltptr(plug_mgr->session_exdata_schema_array, i);
		if (exdata)
		{
			if (schema->free_func)
			{
				schema->free_func(sess, i, exdata, schema->free_arg);
			}
		}
	}
}

struct plugin_manager_runtime *plugin_manager_session_runtime_new(struct plugin_manager_schema *plug_mgr, struct session *sess)
{
	struct plugin_manager_runtime *rt = CALLOC(struct plugin_manager_runtime, 1);
	rt->plug_mgr = plug_mgr;
	rt->sess = sess;
	rt->pending_mq = NULL;
	rt->delivered_mq = NULL;
	rt->session_mq_status=bitmap_new(plug_mgr->topic_num, plug_mgr->subscriber_num, 1);
	rt->plugin_exdata_array = (struct plugin_exdata *)session_exdata_runtime_new(plug_mgr);
	rt->plugin_ctx_array = CALLOC(struct session_plugin_ctx_runtime, utarray_len(plug_mgr->registered_session_plugin_array));
	return rt;

}

void plugin_manager_session_runtime_free(struct plugin_manager_runtime *rt)
{
	if(rt==NULL)return;
	if(rt->pending_mq != NULL)
	{
		session_mq_free(rt->pending_mq, rt->plug_mgr->session_mq_schema_array);
		rt->pending_mq=NULL;
	}
	if(rt->delivered_mq != NULL)
	{
		session_mq_free(rt->delivered_mq, rt->plug_mgr->session_mq_schema_array);
		rt->delivered_mq=NULL;
	}	
	if(rt->session_mq_status != NULL)
	{
		bitmap_free(rt->session_mq_status);
	}
	unsigned int len = utarray_len(rt->plug_mgr->registered_session_plugin_array);
	for(unsigned int i = 0; i < len; i++)
	{
		struct session_plugin_ctx_runtime *plugin_ctx_rt=(rt->plugin_ctx_array+i);
		struct registered_session_plugin_schema *session_plugin_schema = (struct registered_session_plugin_schema *)utarray_eltptr(rt->plug_mgr->registered_session_plugin_array, i);
		if(session_plugin_schema->on_ctx_free && plugin_ctx_rt->state==ACTIVE)
		{
			session_plugin_schema->on_ctx_free(rt->sess, plugin_ctx_rt->plugin_ctx, session_plugin_schema->plugin_env);
		}
	}
	FREE(rt->plugin_ctx_array);

	session_exdata_runtime_free(rt->plug_mgr, rt->sess, rt->plugin_exdata_array);
	FREE(rt->plugin_exdata_array);
	FREE(rt);
}


/*********************************************
 * PLUGIN MANAGER PACKET PLUGIN *
 *********************************************/


UT_icd registered_packet_plugin_array_icd = {sizeof(struct registered_packet_plugin_schema), NULL, NULL, NULL};

int stellar_packet_plugin_register(struct stellar *st, unsigned char ip_protocol, plugin_on_packet_func on_packet, void *plugin_env)
{
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	if(plug_mgr->registered_packet_plugin_array == NULL)
	{
		utarray_new(plug_mgr->registered_packet_plugin_array, &registered_packet_plugin_array_icd);
	}
	struct registered_packet_plugin_schema packet_plugin_schema;
	memset(&packet_plugin_schema, 0, sizeof(packet_plugin_schema));
	packet_plugin_schema.ip_protocol = ip_protocol;
	packet_plugin_schema.on_packet = on_packet;
	packet_plugin_schema.plugin_env = plugin_env;
	utarray_push_back(plug_mgr->registered_packet_plugin_array, &packet_plugin_schema);
	return (PACKET_PULGIN_ID_BASE+utarray_len(plug_mgr->registered_packet_plugin_array));// return packet plugin_id
}

void plugin_manager_on_packet(struct plugin_manager_schema *plug_mgr, struct packet *pkt)
{
	if(plug_mgr->registered_packet_plugin_array == NULL || pkt == NULL)return;
	struct registered_packet_plugin_schema *p=NULL;
	unsigned char ip_proto=packet_get_ip_protocol(pkt);
	while ((p = (struct registered_packet_plugin_schema *)utarray_next(plug_mgr->registered_packet_plugin_array, p)))
	{
		if(p->ip_protocol == ip_proto && p->on_packet)
		{
			p->on_packet(pkt, ip_proto, p->plugin_env);
		}
	}
	return;
}

/*********************************************
 * PLUGIN MANAGER POLLING PLUGIN *
 *********************************************/


UT_icd registered_polling_plugin_array_icd = {sizeof(struct registered_polling_plugin_schema), NULL, NULL, NULL};

int stellar_polling_plugin_register(struct stellar *st, plugin_on_polling_func on_polling, void *plugin_env)
{
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	if(plug_mgr->registered_polling_plugin_array == NULL)
	{
		utarray_new(plug_mgr->registered_polling_plugin_array, &registered_polling_plugin_array_icd);
	}
	struct registered_polling_plugin_schema polling_plugin_schema;
	memset(&polling_plugin_schema, 0, sizeof(polling_plugin_schema));
	polling_plugin_schema.on_polling = on_polling;
	polling_plugin_schema.plugin_env = plugin_env;
	utarray_push_back(plug_mgr->registered_polling_plugin_array, &polling_plugin_schema);
	return (POLLING_PULGIN_ID_BASE+utarray_len(plug_mgr->registered_polling_plugin_array));// return polling plugin_id
}

int plugin_manager_on_polling(struct plugin_manager_schema *plug_mgr)
{
	if(plug_mgr->registered_polling_plugin_array == NULL)return 0;
	struct registered_polling_plugin_schema *p=NULL;
	int polling_state=0;
	while ((p = (struct registered_polling_plugin_schema *)utarray_next(plug_mgr->registered_polling_plugin_array, p)))
	{
		if(p->on_polling)
		{
			if(p->on_polling(p->plugin_env)==1)
			{
				polling_state=1;
			}
		}
	}
	return polling_state;
}

/*********************************************
 * PLUGIN MANAGER SESSION PLUGIN *
 *********************************************/



UT_icd registered_session_plugin_schema_icd = {sizeof(struct registered_session_plugin_schema), NULL, NULL, NULL};


int stellar_session_plugin_register(struct stellar *st, 
									session_ctx_new_func session_ctx_new, 
									session_ctx_free_func session_ctx_free, 
									void *plugin_env)
{
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	if(plug_mgr->registered_session_plugin_array == NULL)
	{
		utarray_new(plug_mgr->registered_session_plugin_array, &registered_session_plugin_schema_icd);
	}
	struct registered_session_plugin_schema session_plugin_schema;
	memset(&session_plugin_schema, 0, sizeof(struct registered_session_plugin_schema));
	session_plugin_schema.on_ctx_new = session_ctx_new;
	session_plugin_schema.on_ctx_free = session_ctx_free;
	session_plugin_schema.plugin_env = plugin_env;
	utarray_push_back(plug_mgr->registered_session_plugin_array, &session_plugin_schema);
	return (utarray_len(plug_mgr->registered_session_plugin_array)-1);// return session plugin_id, equals to session plugin arrary index
}

void plugin_manager_on_session_ingress(struct session *sess,const struct packet *pkt)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	if(plug_mgr_rt==NULL)return;
	int topic_id = -1;
	switch (packet_get_type(pkt))
	{
	case TCP:
		topic_id=plug_mgr_rt->plug_mgr->tcp_topic_id;
		break;
	case TCP_STREAM:
		topic_id=plug_mgr_rt->plug_mgr->tcp_stream_topic_id;
		break;	
	case UDP:
		topic_id=plug_mgr_rt->plug_mgr->udp_topic_id;
		break;			
	case CONTROL:
		topic_id=plug_mgr_rt->plug_mgr->control_packet_topic_id;
		break;			
	default:
		break;
	}
	//TODO: check TCP topic active subscirber num, if 0, return APP_STATE_DROPME, to reduce tcp reassemble overhead
	session_mq_publish_message(sess, topic_id ,(void *)pkt);
	plugin_manager_session_message_dispatch(sess);
    return;
}

void plugin_manager_on_session_egress(struct session *sess,const struct packet *pkt)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	if(plug_mgr_rt==NULL)return;
	session_mq_publish_message(sess, plug_mgr_rt->plug_mgr->egress_topic_id ,(void *)pkt);
	plugin_manager_session_message_dispatch(sess);
	session_mq_free(plug_mgr_rt->delivered_mq, plug_mgr_rt->plug_mgr->session_mq_schema_array);
	plug_mgr_rt->delivered_mq=NULL;
    return;
}

void stellar_session_plugin_dettach_current_session(struct session *sess)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	struct registered_session_plugin_schema *session_plugin_schema = (struct registered_session_plugin_schema *)utarray_eltptr(plug_mgr_rt->plug_mgr->registered_session_plugin_array, (unsigned int)plug_mgr_rt->current_session_plugin_id);
	if(session_plugin_schema==NULL)return;

	unsigned int plugin_subscriber_num = utarray_len(session_plugin_schema->registed_session_mq_subscriber_info);
	//FIXME: maybe no need to clear session_mq_status, check plugin_ctx before message dispatch
	if(plug_mgr_rt->session_mq_status)
	{
		for(unsigned int i=0; i < plugin_subscriber_num; i++)
		{
			struct session_mq_subscriber_info *session_plugin_sub_info = (struct session_mq_subscriber_info *)utarray_eltptr(session_plugin_schema->registed_session_mq_subscriber_info, i);
				bitmap_set(plug_mgr_rt->session_mq_status, session_plugin_sub_info->topic_id, session_plugin_sub_info->subscriber_idx, 0);
		}
	}	

	if(session_plugin_schema->on_ctx_free)
	{
		session_plugin_schema->on_ctx_free(sess, (plug_mgr_rt->plugin_ctx_array+plug_mgr_rt->current_session_plugin_id)->plugin_ctx, session_plugin_schema->plugin_env);
	}
	(plug_mgr_rt->plugin_ctx_array+plug_mgr_rt->current_session_plugin_id)->plugin_ctx=NULL;
	(plug_mgr_rt->plugin_ctx_array+plug_mgr_rt->current_session_plugin_id)->state=EXIT;
	return;
}

