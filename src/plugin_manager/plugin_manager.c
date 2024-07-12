#include "plugin_manager_interna.h"

#include "stellar_internal.h"

#include "stellar/session.h"

#include "stellar/utils.h"
#include "toml/toml.h"
#include "uthash/utlist.h"


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
	struct plugin_manager_schema *plug_mgr = CALLOC(struct plugin_manager_schema, 1);
	plug_mgr->max_message_dispatch=MAX_MSG_PER_DISPATCH;
	if(spec_num > 0)
	{
		utarray_new(plug_mgr->plugin_load_specs_array,&plugin_specs_icd);
		utarray_reserve(plug_mgr->plugin_load_specs_array, spec_num);
	}

	plug_mgr->st = st;
	stellar_plugin_manager_schema_set(st, plug_mgr);


	plug_mgr->tcp_topic_id=stellar_session_mq_create_topic(st, TOPIC_TCP, NULL, NULL);
	plug_mgr->tcp_stream_topic_id=stellar_session_mq_create_topic(st, TOPIC_TCP_STREAM, NULL, NULL);
	plug_mgr->udp_topic_id=stellar_session_mq_create_topic(st, TOPIC_UDP, NULL, NULL);
	plug_mgr->egress_topic_id=stellar_session_mq_create_topic(st, TOPIC_EGRESS, NULL, NULL);
	plug_mgr->control_packet_topic_id=stellar_session_mq_create_topic(st, TOPIC_CONTROL_PACKET, NULL, NULL);

	for(int i = 0; i < spec_num; i++)
	{
		if (specs[i].load_cb != NULL)
		{
			specs[i].plugin_ctx=specs[i].load_cb(st);
			utarray_push_back(plug_mgr->plugin_load_specs_array, &specs[i]);
		}
	}
	FREE(specs);
	return plug_mgr;
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
	if(plug_mgr->registered_polling_plugin_array)utarray_free(plug_mgr->registered_polling_plugin_array);
	if(plug_mgr->registered_packet_plugin_array)
	{
        struct registered_packet_plugin_schema *s = NULL;
        while ((s = (struct registered_packet_plugin_schema *)utarray_next(plug_mgr->registered_packet_plugin_array, s)))
        {
			if(s->registed_packet_mq_subscriber_info)utarray_free(s->registed_packet_mq_subscriber_info);
        }
        utarray_free(plug_mgr->registered_packet_plugin_array);
	}	
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
 * STELLAR EXDATA *
 *******************************/

static void stellar_exdata_met_copy(void *_dst, const void *_src)
{
    struct stellar_exdata_schema *dst = (struct stellar_exdata_schema *)_dst, *src = (struct stellar_exdata_schema *)_src;
    dst->sess_free_func = src->sess_free_func;
    dst->free_arg = src->free_arg;
    dst->idx = src->idx;
    dst->name = src->name ? strdup(src->name) : NULL;
}

static void stellar_exdata_met_dtor(void *_elt)
{
    struct stellar_exdata_schema *elt = (struct stellar_exdata_schema *)_elt;
    if (elt->name)
        FREE(elt->name);
}

UT_icd stellar_exdata_meta_icd = {sizeof(struct stellar_exdata_schema), NULL, stellar_exdata_met_copy, stellar_exdata_met_dtor};

int stellar_exdata_new_index(struct stellar *st, const char *name, UT_array **exdata_schema, void *free_func,void *free_arg) 
{
	if(st==NULL || name==NULL || exdata_schema==NULL)return -1;
	if(*exdata_schema == NULL)
	{
		utarray_new(*exdata_schema, &stellar_exdata_meta_icd);
	}
	if(*exdata_schema == NULL)return -1;
	unsigned int len = utarray_len(*exdata_schema);
	struct stellar_exdata_schema *t_schema;
	for(unsigned int i = 0; i < len; i++)
	{
		t_schema = (struct stellar_exdata_schema *)utarray_eltptr(*exdata_schema, i);
		if(strcmp(t_schema->name, name) == 0)
		{
			t_schema->free_func=free_func;
			t_schema->free_arg=free_arg;
			return t_schema->idx;
		}
	}
	struct stellar_exdata_schema new_schema;
	memset(&new_schema, 0, sizeof(struct stellar_exdata_schema));
	new_schema.free_func=free_func;
	new_schema.name=(char *)name;
	new_schema.idx=len;
	new_schema.free_arg=free_arg;
	utarray_push_back(*exdata_schema, &new_schema);
	return new_schema.idx;
}

int stellar_exdata_set(UT_array *exdata_schema, struct stellar_exdata *exdata_array, int idx, void *ex_ptr)
{
	if(exdata_schema == NULL|| exdata_array == NULL)return -1;
	unsigned int len=utarray_len(exdata_schema);
	if(len < (unsigned int)idx)return -1;
	(exdata_array+idx)->exdata=ex_ptr;
	return 0;
}

void *stellar_exdata_get(UT_array *exdata_schema, struct stellar_exdata *exdata_array, int idx)
{
	if(exdata_schema == NULL|| exdata_array == NULL)return NULL;
	unsigned int len = utarray_len(exdata_schema);
	if(len < (unsigned int)idx)return NULL;
	return (exdata_array+idx)->exdata;
}

/*******************************
 * SESSION EXDATA *
 *******************************/

int stellar_session_exdata_new_index(struct stellar *st, const char *name, session_exdata_free *free_func,void *free_arg) 
{
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	assert(plug_mgr);
	return stellar_exdata_new_index(st, name, &plug_mgr->session_exdata_schema_array, (void*)free_func, free_arg);
}

int session_exdata_set(struct session *sess, int idx, void *ex_ptr)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	if(plug_mgr_rt == NULL)return -1;
	if(plug_mgr_rt->plug_mgr->session_exdata_schema_array == NULL)return -1;
	return stellar_exdata_set(plug_mgr_rt->plug_mgr->session_exdata_schema_array, plug_mgr_rt->sess_exdata_array, idx, ex_ptr);
}

void *session_exdata_get(struct session *sess, int idx)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	if(plug_mgr_rt == NULL)return NULL;
	if(plug_mgr_rt->plug_mgr->session_exdata_schema_array==NULL)return NULL;
	return stellar_exdata_get(plug_mgr_rt->plug_mgr->session_exdata_schema_array, plug_mgr_rt->sess_exdata_array, idx);
}

/*******************************
 * STELLAR MQ *
 *******************************/

static void stellar_mq_topic_schema_copy(void *_dst, const void *_src)
{
    struct stellar_mq_topic_schema *dst = (struct stellar_mq_topic_schema *)_dst,
                                   *src = (struct stellar_mq_topic_schema *)_src;
    dst->subscribers = src->subscribers;
    dst->free_cb = src->free_cb;
    dst->free_cb_arg = src->free_cb_arg;
    dst->topic_id = src->topic_id;
    dst->subscriber_cnt = src->subscriber_cnt;
    dst->topic_name = src->topic_name ? strdup(src->topic_name) : NULL;
}

static void stellar_mq_topic_schema_dtor(void *_elt)
{
    struct stellar_mq_topic_schema *elt = (struct stellar_mq_topic_schema *)_elt;
    if (elt->topic_name)
        FREE(elt->topic_name);
    // FREE(elt); // free the item
}

UT_icd stellar_mq_topic_schema_icd = {sizeof(struct stellar_mq_topic_schema), NULL, stellar_mq_topic_schema_copy, stellar_mq_topic_schema_dtor};

int stellar_mq_get_topic_id(const char *topic_name, UT_array *mq_schema_array)
{
	if(topic_name == NULL || mq_schema_array == NULL )return -1;
	unsigned int len = utarray_len(mq_schema_array);
	struct stellar_mq_topic_schema *t_schema;
	for(unsigned int i = 0; i < len; i++)
	{
		t_schema = (struct stellar_mq_topic_schema *)utarray_eltptr(mq_schema_array, i);
		if(strcmp(t_schema->topic_name, topic_name) == 0)
		{
			return i;
		}
	}	
	return -1;
}

int stellar_mq_update_topic(int topic_id, void *msg_free_cb, void *msg_free_arg, UT_array *mq_schema_array)
{
	if(mq_schema_array == NULL)return -1;
	unsigned int len = utarray_len(mq_schema_array);
	if(len < (unsigned int)topic_id)return -1;
	struct stellar_mq_topic_schema *t_schema = (struct stellar_mq_topic_schema *)utarray_eltptr(mq_schema_array, (unsigned int)topic_id);
	if(t_schema == NULL)return -1;
	t_schema->free_cb=msg_free_cb;
	t_schema->free_cb_arg=msg_free_arg;
	return 0;
}

int stellar_mq_create_topic(struct stellar *st, const char *topic_name, void *msg_free_cb, void *msg_free_arg, UT_array **mq_schema_array)
{
	if(*mq_schema_array == NULL)
	{
		utarray_new(*mq_schema_array, &stellar_mq_topic_schema_icd);
	}
	unsigned int len = utarray_len(*mq_schema_array);
	if(stellar_mq_get_topic_id(topic_name, *mq_schema_array) >= 0)
	{
		return -1;
	}
	struct stellar_mq_topic_schema t_schema;
	memset(&t_schema, 0, sizeof(struct stellar_mq_topic_schema));
	t_schema.free_cb=msg_free_cb;
	t_schema.topic_name=(char *)topic_name;
	t_schema.topic_id=len;//topid_id equals arrary index
	t_schema.free_cb_arg=msg_free_arg;
	t_schema.subscribers=NULL;
	t_schema.subscriber_cnt=0;
	utarray_push_back(*mq_schema_array, &t_schema);
	return t_schema.topic_id;	
}

int stellar_mq_destroy_topic(int topic_id, UT_array *mq_schema_array)
{
	if(mq_schema_array==NULL)return -1;
    unsigned int len = utarray_len(mq_schema_array);
    if (len <= (unsigned int)topic_id)
        return -1;
    struct stellar_mq_topic_schema *topic =
        (struct stellar_mq_topic_schema *)utarray_eltptr(mq_schema_array, (unsigned int)topic_id);
    struct stellar_mq_subscriber *sub_elt, *sub_tmp;

	if(topic == NULL)return -1;

    if (topic->is_destroyed == 1)return 0;

    DL_FOREACH_SAFE(topic->subscribers, sub_elt, sub_tmp)
    {
        DL_DELETE(topic->subscribers, sub_elt);
        FREE(sub_elt);
    }
    topic->is_destroyed = 1;
    return 1; // success
}

static int stellar_mq_publish_message(int topic_id, void *data, UT_array *mq_schema_array, struct stellar_message *mq[], enum session_mq_priority priority)
{
	if(mq_schema_array==NULL || topic_id < 0)return -1;
    unsigned int len = utarray_len(mq_schema_array);
    if (len <= (unsigned int)topic_id)return -1;
	struct stellar_message *msg= CALLOC(struct stellar_message,1);
	msg->header.topic_id = topic_id;
	msg->header.priority = priority;
	msg->body = data;
	DL_APPEND(mq[priority], msg);
	return 0;
}

UT_icd stellar_mq_subscriber_info_icd = {sizeof(struct stellar_mq_subscriber_info), NULL, NULL, NULL};


/*******************************
 * SESSION MQ *
 *******************************/

void session_mq_free(struct session *sess, struct stellar_message **head, UT_array *mq_schema_array)
{
	struct stellar_message *mq_elt, *tmp;
	struct stellar_mq_topic_schema *topic;
    DL_FOREACH_SAFE(*head, mq_elt, tmp)
    {
		topic = (struct stellar_mq_topic_schema *)utarray_eltptr(mq_schema_array,
                                                                    (unsigned int)(mq_elt->header.topic_id));
        if (topic && topic->free_cb)
        {
            topic->sess_msg_free_cb(sess, mq_elt->body, topic->free_cb_arg);
        }
        DL_DELETE(*head, mq_elt);
        FREE(mq_elt);
    }
	FREE(*head);
}

inline int stellar_session_mq_get_topic_id(struct stellar *st, const char *topic_name)
{
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	assert(plug_mgr);
	return stellar_mq_get_topic_id(topic_name, plug_mgr->session_mq_schema_array);
}

int stellar_session_mq_update_topic(struct stellar *st, int topic_id, session_msg_free_cb_func *msg_free_cb, void *msg_free_arg)
{
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	assert(plug_mgr);
	return stellar_mq_update_topic(topic_id, (void *)msg_free_cb, msg_free_arg, plug_mgr->session_mq_schema_array);
}

int stellar_session_mq_create_topic(struct stellar *st, const char *topic_name, session_msg_free_cb_func *msg_free_cb, void *msg_free_arg)
{
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	assert(plug_mgr);
	int topic_id=stellar_mq_create_topic(st, topic_name, (void *)msg_free_cb, msg_free_arg, &plug_mgr->session_mq_schema_array);
	if(topic_id>=0)plug_mgr->session_mq_topic_num+=1;
	return topic_id;	
}

int stellar_session_mq_destroy_topic(struct stellar *st, int topic_id)
{
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	assert(plug_mgr);
	int ret =  stellar_mq_destroy_topic(topic_id, plug_mgr->session_mq_schema_array);
	if(ret==1)plug_mgr->session_mq_topic_num-=1;
	return ret;
}



int session_mq_publish_message_with_priority(struct session *sess, int topic_id, void *data, enum session_mq_priority priority)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	assert(plug_mgr_rt);
	if(plug_mgr_rt->session_mq_status==NULL)return -1;//runtime free stage , mq_status alaway null, ignore publish message
	if(plug_mgr_rt->pub_session_msg_cnt >= plug_mgr_rt->plug_mgr->max_message_dispatch)return -1;
	int ret=stellar_mq_publish_message(topic_id, data, plug_mgr_rt->plug_mgr->session_mq_schema_array, plug_mgr_rt->priority_mq, priority);
	if(ret==0)plug_mgr_rt->pub_session_msg_cnt+=1;
	return ret;
}


int session_mq_publish_message(struct session *sess, int topic_id, void *data)
{
	return session_mq_publish_message_with_priority(sess, topic_id, data, SESSION_MQ_PRIORITY_NORMAL);
}

static void session_mq_update_topic_status(struct plugin_manager_runtime *plug_mgr_rt, struct stellar_mq_topic_schema *topic)
{
		//update topic status
		switch (bitmap_is_all_zero(plug_mgr_rt->session_mq_status, 0, topic->topic_id, topic->subscriber_cnt))
		{
		case 1:
			bitmap_set(plug_mgr_rt->session_topic_status, 0, topic->topic_id, 0);
			break;
		case 0:
			bitmap_set(plug_mgr_rt->session_topic_status, 0, topic->topic_id, 1);
			break;
		default:
			break;
		}
		return;
}

static int session_mq_set_message_status(struct session *sess, int topic_id, int plugin_id, int bit_value)
{
	if(bit_value!=0 && bit_value!=1)return -1;
	if(plugin_id >= PACKET_PULGIN_ID_BASE)return -1;// ignore packet plugin
	if(topic_id < 0 || plugin_id < 0)return -1;
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	if(plug_mgr_rt==NULL)return -1;
	if(topic_id >= plug_mgr_rt->plug_mgr->session_mq_topic_num)return -1;// topic_id out of range
	struct stellar_mq_topic_schema *topic = (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr_rt->plug_mgr->session_mq_schema_array, (unsigned int)topic_id);
	if(topic==NULL)return -1;

	struct registered_session_plugin_schema *session_plugin_schema = (struct registered_session_plugin_schema *)utarray_eltptr(plug_mgr_rt->plug_mgr->registered_session_plugin_array, (unsigned int)plugin_id);
	if(session_plugin_schema==NULL)return -1;

	unsigned int plugin_subscriber_num = utarray_len(session_plugin_schema->registed_session_mq_subscriber_info);
	if(plug_mgr_rt->session_mq_status)
	{
		for(unsigned int i=0; i < plugin_subscriber_num; i++)
		{
			struct stellar_mq_subscriber_info *session_plugin_sub_info = (struct stellar_mq_subscriber_info *)utarray_eltptr(session_plugin_schema->registed_session_mq_subscriber_info, i);
			if(topic_id==session_plugin_sub_info->topic_id)
			{
				bitmap_set(plug_mgr_rt->session_mq_status, session_plugin_sub_info->subscriber_idx, topic_id, bit_value);
			}
		}
		session_mq_update_topic_status(plug_mgr_rt, topic);
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


int stellar_session_mq_subscribe(struct stellar *st, int topic_id, on_session_msg_cb_func *plugin_on_msg_cb, int plugin_id)
{
	if(plugin_id >= PACKET_PULGIN_ID_BASE)return -1;// ignore packet plugin
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	if(plug_mgr == NULL || plug_mgr->session_mq_schema_array==NULL || plug_mgr->registered_session_plugin_array == NULL)return -1;

    unsigned int len = utarray_len(plug_mgr->session_mq_schema_array);
    if (len <= (unsigned int)topic_id)return -1;

	struct registered_session_plugin_schema *session_plugin_schema = (struct registered_session_plugin_schema *)utarray_eltptr(plug_mgr->registered_session_plugin_array, (unsigned)plugin_id);
	if(session_plugin_schema==NULL)return -1;

    struct stellar_mq_topic_schema *topic = (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr->session_mq_schema_array, (unsigned int)topic_id);
	if(topic==NULL)return -1;

	if(session_plugin_schema->registed_session_mq_subscriber_info==NULL)
	{
		utarray_new(session_plugin_schema->registed_session_mq_subscriber_info, &stellar_mq_subscriber_info_icd);
	}

	// if plugin already subscribe current topic, return 0
	struct stellar_mq_subscriber_info *p=NULL;
  	while( (p=(struct stellar_mq_subscriber_info *)utarray_next(session_plugin_schema->registed_session_mq_subscriber_info,p))) 
	{
		if(p->topic_id==topic_id)
		{
			struct stellar_mq_subscriber *tmp_subscriber=topic->subscribers;
			int cnt=0;
			while(tmp_subscriber)
			{
				if(cnt==p->subscriber_idx)
				{
					tmp_subscriber->sess_msg_cb=plugin_on_msg_cb;
					return 0;
				}
				cnt++;
				tmp_subscriber=tmp_subscriber->next;
			}
		}
	};

	struct stellar_mq_subscriber *new_subscriber = CALLOC(struct stellar_mq_subscriber,1);
	new_subscriber->topic_subscriber_idx = topic->subscriber_cnt;
	new_subscriber->plugin_idx = plugin_id;
    new_subscriber->sess_msg_cb = plugin_on_msg_cb;
	DL_APPEND(topic->subscribers, new_subscriber);

	struct stellar_mq_subscriber_info sub_info;
	memset(&sub_info, 0, sizeof(struct stellar_mq_subscriber_info));
	sub_info.topic_id=topic_id;
	sub_info.subscriber_idx=topic->subscriber_cnt;	
	utarray_push_back(session_plugin_schema->registed_session_mq_subscriber_info, &sub_info);
	topic->subscriber_cnt+=1;
	plug_mgr->session_topic_subscriber_num+=1;
	return 0;
}

static void session_mq_dispatch_one_message(struct session *sess, struct stellar_message *mq_elt)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
    struct stellar_mq_subscriber *sub_elt, *sub_tmp;
	struct registered_session_plugin_schema *session_plugin_schema;
	struct session_plugin_ctx_runtime *plugin_ctx_rt;
    struct stellar_mq_topic_schema *topic = (struct stellar_mq_topic_schema *)utarray_eltptr(
        plug_mgr_rt->plug_mgr->session_mq_schema_array, (unsigned int)(mq_elt->header.topic_id));

    if (topic)
    {
        int cur_sub_idx = 0;
        DL_FOREACH_SAFE(topic->subscribers, sub_elt, sub_tmp)
        {
            plug_mgr_rt->current_session_plugin_id = sub_elt->plugin_idx;
            if (bitmap_get(plug_mgr_rt->session_mq_status, cur_sub_idx, mq_elt->header.topic_id) != 0)
            {
                plugin_ctx_rt = (plug_mgr_rt->plugin_ctx_array + sub_elt->plugin_idx);
                session_plugin_schema = (struct registered_session_plugin_schema *)utarray_eltptr(
                    plug_mgr_rt->plug_mgr->registered_session_plugin_array, (unsigned int)sub_elt->plugin_idx);
                if (session_plugin_schema)
                {
                    if (plugin_ctx_rt->state == INIT)
                    {
                        if (session_plugin_schema->on_ctx_new)
                        {
                            plugin_ctx_rt->plugin_ctx =
                                session_plugin_schema->on_ctx_new(sess, session_plugin_schema->plugin_env);
                            if (plugin_ctx_rt->state == EXIT && session_plugin_schema->on_ctx_free)
                            {
                                session_plugin_schema->on_ctx_free(sess, plugin_ctx_rt->plugin_ctx,
                                                                session_plugin_schema->plugin_env);
                                plugin_ctx_rt->plugin_ctx = NULL;
                            }
                            else
                            {
                                plugin_ctx_rt->state = ACTIVE;
                            }
                        }
                    }
                    if (sub_elt->sess_msg_cb &&
                        bitmap_get(plug_mgr_rt->session_mq_status, cur_sub_idx, mq_elt->header.topic_id) !=
                            0) // ctx_new maybe call detach, need check again
                    {
                        sub_elt->sess_msg_cb(sess, mq_elt->header.topic_id, mq_elt->body, plugin_ctx_rt->plugin_ctx,
                                            session_plugin_schema->plugin_env);
                    }
                }
            }
            cur_sub_idx++;
        }
        if (cur_sub_idx == 0)
            bitmap_set(plug_mgr_rt->session_topic_status, 0, mq_elt->header.topic_id, 0);
    }
}

static void plugin_manager_session_message_dispatch(struct session *sess)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	if(plug_mgr_rt==NULL)return;

	struct stellar_message *mq_elt=NULL, *mq_tmp=NULL;

	int cur_priority = SESSION_MQ_PRIORITY_HIGH;
	while(cur_priority >= SESSION_MQ_PRIORITY_LOW)
	{
		if(plug_mgr_rt->priority_mq[cur_priority]==NULL)
		{
			cur_priority--;
			continue;
		}
        DL_FOREACH_SAFE(plug_mgr_rt->priority_mq[cur_priority], mq_elt, mq_tmp)
        {
			session_mq_dispatch_one_message(sess, mq_elt);
			DL_DELETE(plug_mgr_rt->priority_mq[mq_elt->header.priority], mq_elt);
    		DL_APPEND(plug_mgr_rt->dealth_letter_queue, mq_elt); // move to dlq list

			cur_priority=SESSION_MQ_PRIORITY_HIGH;
			break;
        }		
	}

#if 0
    while (plug_mgr_rt->pending_mq != NULL)
    {
        DL_FOREACH_SAFE(plug_mgr_rt->pending_mq, mq_elt, mq_tmp)
        {
			session_mq_dispatch_one_message(sess, mq_elt);
			DL_DELETE(plug_mgr_rt->pending_mq[mq_elt->header.priority], mq_elt);
    		DL_APPEND(plug_mgr_rt->delivered_mq, mq_elt); // move to delivered message list
        }
    }
#endif
	return;
}

int session_mq_topic_is_active(struct session *sess, int topic_id)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	assert(plug_mgr_rt);
	if(plug_mgr_rt->session_topic_status==NULL)return -1;//runtime free stage , mq_status alaway null, ignore publish message	
	if(topic_id >= plug_mgr_rt->plug_mgr->session_mq_topic_num)return -1;// topic_id out of range
	if(bitmap_get(plug_mgr_rt->session_topic_status, 0, topic_id) == 0)return 0;
	return 1;
}

/*******************************
 * PLUGIN MANAGER SESSION RUNTIME *
 *******************************/

static struct stellar_exdata *session_exdata_runtime_new(struct plugin_manager_schema *plug_mgr)
{
	struct stellar_exdata *exdata_rt = NULL;
	if(plug_mgr->session_exdata_schema_array==NULL)return NULL;
	unsigned int len = utarray_len(plug_mgr->session_exdata_schema_array);
	if(len > 0)
	{
		exdata_rt=CALLOC(struct stellar_exdata, len);
	}
	return exdata_rt;
}

static void session_exdata_runtime_free(struct plugin_manager_schema *plug_mgr, struct session *sess, struct stellar_exdata *exdata_rt)
{
	if(exdata_rt==NULL)return;
	if(plug_mgr->session_exdata_schema_array==NULL)return;
	unsigned int len=utarray_len(plug_mgr->session_exdata_schema_array);
	for (unsigned int i = 0; i < len; i++)
	{
		void *exdata = (exdata_rt + i)->exdata;
		struct stellar_exdata_schema *schema = (struct stellar_exdata_schema *)utarray_eltptr(plug_mgr->session_exdata_schema_array, i);
		if (exdata)
		{
			if (schema->sess_free_func)
			{
				schema->sess_free_func(sess, i, exdata, schema->free_arg);
			}
		}
	}
}

struct plugin_manager_runtime *plugin_manager_session_runtime_new(struct plugin_manager_schema *plug_mgr, struct session *sess)
{
	struct plugin_manager_runtime *rt = CALLOC(struct plugin_manager_runtime, 1);
	rt->plug_mgr = plug_mgr;
	rt->sess = sess;
	memset(rt->priority_mq, 0, sizeof(rt->priority_mq));
	rt->dealth_letter_queue = NULL;
	rt->session_mq_status=bitmap_new(plug_mgr->session_topic_subscriber_num, plug_mgr->session_mq_topic_num, 1);
	rt->session_topic_status=bitmap_new(1, plug_mgr->session_mq_topic_num, 1);
	rt->sess_exdata_array = (struct stellar_exdata *)session_exdata_runtime_new(plug_mgr);
	if(plug_mgr->registered_session_plugin_array)
		rt->plugin_ctx_array = CALLOC(struct session_plugin_ctx_runtime, utarray_len(plug_mgr->registered_session_plugin_array));
	return rt;

}

void plugin_manager_session_runtime_free(struct plugin_manager_runtime *rt)
{
	if(rt==NULL)return;
	assert(rt->priority_mq[SESSION_MQ_PRIORITY_HIGH]==NULL);
	assert(rt->priority_mq[SESSION_MQ_PRIORITY_NORMAL]==NULL);
	assert(rt->priority_mq[SESSION_MQ_PRIORITY_LOW]==NULL);
	for(int i=0; i < SESSION_MQ_PRIORITY_MAX; i++)
	{
		if(rt->priority_mq[i] != NULL)
		{
			session_mq_free(rt->sess, &rt->priority_mq[i], rt->plug_mgr->session_mq_schema_array);
		}
	}
	assert(rt->dealth_letter_queue==NULL);
	if(rt->dealth_letter_queue != NULL)
	{
		session_mq_free(rt->sess, &rt->dealth_letter_queue, rt->plug_mgr->session_mq_schema_array);
	}	
	if(rt->session_mq_status != NULL)
	{
		bitmap_free(rt->session_mq_status);
		rt->session_mq_status=NULL;
	}
	if(rt->session_topic_status != NULL)
	{
		bitmap_free(rt->session_topic_status);
		rt->session_topic_status=NULL;
	}
    if (rt->plug_mgr->registered_session_plugin_array)
    {
        unsigned int len = utarray_len(rt->plug_mgr->registered_session_plugin_array);
        for (unsigned int i = 0; i < len; i++)
        {
            struct session_plugin_ctx_runtime *plugin_ctx_rt = (rt->plugin_ctx_array + i);
            struct registered_session_plugin_schema *session_plugin_schema =
                (struct registered_session_plugin_schema *)utarray_eltptr(rt->plug_mgr->registered_session_plugin_array, i);
            if (session_plugin_schema->on_ctx_free && plugin_ctx_rt->state == ACTIVE)
            {
                session_plugin_schema->on_ctx_free(rt->sess, plugin_ctx_rt->plugin_ctx,
                                                   session_plugin_schema->plugin_env);
            }
        }
        FREE(rt->plugin_ctx_array);
    }
    session_exdata_runtime_free(rt->plug_mgr, rt->sess, rt->sess_exdata_array);
	FREE(rt->sess_exdata_array);
	FREE(rt);
}


/*********************************************
 * PLUGIN MANAGER PACKET PLUGIN *
 *********************************************/



UT_icd registered_packet_plugin_array_icd = {sizeof(struct registered_packet_plugin_schema), NULL, NULL, NULL};

int stellar_packet_plugin_register(struct stellar *st, unsigned char ip_proto, plugin_on_packet_func on_packet_cb, void *plugin_env)
{
	struct plugin_manager_schema *plug_mgr = stellar_plugin_manager_schema_get(st);
	if(plug_mgr->registered_packet_plugin_array == NULL)
	{
		utarray_new(plug_mgr->registered_packet_plugin_array, &registered_packet_plugin_array_icd);
	}
	struct registered_packet_plugin_schema packet_plugin_schema;
	memset(&packet_plugin_schema, 0, sizeof(packet_plugin_schema));
	packet_plugin_schema.ip_protocol = ip_proto;
	packet_plugin_schema.on_packet = on_packet_cb;
	packet_plugin_schema.plugin_env = plugin_env;
	utarray_push_back(plug_mgr->registered_packet_plugin_array, &packet_plugin_schema);
	return (PACKET_PULGIN_ID_BASE+utarray_len(plug_mgr->registered_packet_plugin_array)-1);// return packet plugin_id, equals to packet plugin arrary index + PACKET_PULGIN_ID_BASE
}

void plugin_manager_on_packet_ingress(struct plugin_manager_schema *plug_mgr, struct packet *pkt)
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
	return (POLLING_PULGIN_ID_BASE+utarray_len(plug_mgr->registered_polling_plugin_array)-1);// return polling plugin_id, equals to polling plugin arrary index + POLLING_PULGIN_ID_BASE
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

void plugin_manager_on_session_ingress(struct session *sess, struct packet *pkt)
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
	plug_mgr_rt->pub_session_msg_cnt=0;
	session_mq_publish_message(sess, topic_id ,(void *)pkt);
	plugin_manager_session_message_dispatch(sess);
    return;
}

void plugin_manager_on_session_egress(struct session *sess, struct packet *pkt)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	if(plug_mgr_rt==NULL)return;
	session_mq_publish_message(sess, plug_mgr_rt->plug_mgr->egress_topic_id ,pkt);
	plugin_manager_session_message_dispatch(sess);
	session_mq_free(plug_mgr_rt->sess,&plug_mgr_rt->dealth_letter_queue, plug_mgr_rt->plug_mgr->session_mq_schema_array);
	assert(plug_mgr_rt->priority_mq[SESSION_MQ_PRIORITY_HIGH]==NULL);
	assert(plug_mgr_rt->priority_mq[SESSION_MQ_PRIORITY_NORMAL]==NULL);
	assert(plug_mgr_rt->priority_mq[SESSION_MQ_PRIORITY_LOW]==NULL);
	assert(plug_mgr_rt->dealth_letter_queue==NULL);
	return;
}

void plugin_manager_on_session_closing(struct session *sess)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	if(plug_mgr_rt==NULL)return;
	switch (session_get_type(sess))
	{
		case SESSION_TYPE_TCP:
			session_mq_publish_message(sess, plug_mgr_rt->plug_mgr->tcp_topic_id ,NULL);
			session_mq_publish_message(sess, plug_mgr_rt->plug_mgr->tcp_stream_topic_id , NULL);
			break;
		case SESSION_TYPE_UDP:
			session_mq_publish_message(sess, plug_mgr_rt->plug_mgr->udp_topic_id ,NULL);
			break;
		default:
			break;
	}
	plugin_manager_session_message_dispatch(sess);
	session_mq_free(plug_mgr_rt->sess,&plug_mgr_rt->dealth_letter_queue, plug_mgr_rt->plug_mgr->session_mq_schema_array);
	assert(plug_mgr_rt->priority_mq[SESSION_MQ_PRIORITY_HIGH]==NULL);
	assert(plug_mgr_rt->priority_mq[SESSION_MQ_PRIORITY_NORMAL]==NULL);
	assert(plug_mgr_rt->priority_mq[SESSION_MQ_PRIORITY_LOW]==NULL);
	assert(plug_mgr_rt->dealth_letter_queue==NULL);
    return;
}

void stellar_session_plugin_dettach_current_session(struct session *sess)
{
	struct plugin_manager_runtime *plug_mgr_rt = session_plugin_manager_runtime_get(sess);
	struct registered_session_plugin_schema *session_plugin_schema = (struct registered_session_plugin_schema *)utarray_eltptr(plug_mgr_rt->plug_mgr->registered_session_plugin_array, (unsigned int)plug_mgr_rt->current_session_plugin_id);
	if(session_plugin_schema==NULL)return;
	struct stellar_mq_topic_schema *topic=NULL;
	unsigned int plugin_subscriber_num = utarray_len(session_plugin_schema->registed_session_mq_subscriber_info);
	//Won't Do: maybe no need to clear session_mq_status, check plugin_ctx before message dispatch
	//allow plugin register with null ctx_new and ctx_free
	if(plug_mgr_rt->session_mq_status)
	{
		for(unsigned int i=0; i < plugin_subscriber_num; i++)
		{
			struct stellar_mq_subscriber_info *session_plugin_sub_info = (struct stellar_mq_subscriber_info *)utarray_eltptr(session_plugin_schema->registed_session_mq_subscriber_info, i);
			bitmap_set(plug_mgr_rt->session_mq_status,  session_plugin_sub_info->subscriber_idx,session_plugin_sub_info->topic_id, 0);
			topic = (struct stellar_mq_topic_schema *)utarray_eltptr(plug_mgr_rt->plug_mgr->session_mq_schema_array, (unsigned int)session_plugin_sub_info->topic_id);
			session_mq_update_topic_status(plug_mgr_rt, topic);
		}
	}	

	//dettach in ctx INIT, do not call on_ctx_free immidiately
	if(plug_mgr_rt->plugin_ctx_array[plug_mgr_rt->current_session_plugin_id].state != INIT && (session_plugin_schema->on_ctx_free))
	{
		session_plugin_schema->on_ctx_free(sess, plug_mgr_rt->plugin_ctx_array[plug_mgr_rt->current_session_plugin_id].plugin_ctx, session_plugin_schema->plugin_env);
		plug_mgr_rt->plugin_ctx_array[plug_mgr_rt->current_session_plugin_id].plugin_ctx=NULL;
	}
	plug_mgr_rt->plugin_ctx_array[plug_mgr_rt->current_session_plugin_id].state=EXIT;

	return;
}

