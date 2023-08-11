#include "utils.h"
#include "session_manager.h"

#include "uthash/uthash.h"
#include "uthash/utarray.h"
#include <sys/queue.h>

/*
struct session_mq_subscriber 
{
    msg_consume_cb_func *read_cb;
    void *cb_arg;
};
UT_icd session_mq_subscribers_icd = {sizeof(struct session_mq_subscriber), NULL, NULL, NULL};


struct session_mq_topic_schema
{
	char *topic_name;
	msg_free_cb_func *free_cb;
	void *cb_arg;
	UT_array *subscribers;	
	UT_hash_handle hh;
};
*/



struct session_exdata_meta
{
	const char *name;
	session_ex_free *free_func;
	void *arg;
	int idx;
};
UT_icd session_exdata_meta_icd = {sizeof(struct session_exdata_meta), NULL, NULL, NULL};
UT_array *global_session_exdata_metas = NULL;

struct session_exdata_runtime
{
	struct session *sess;
	void *exdata;
};
UT_icd session_exdata_runtime_icd = {sizeof(struct session_exdata_runtime), NULL, NULL, NULL};


int session_get_ex_new_index(const char *name, session_ex_free *free_func,void *arg)
{
	if(global_session_exdata_metas == NULL)
	{
		utarray_new(global_session_exdata_metas, &session_exdata_meta_icd);
	}
	unsigned int len = utarray_len(global_session_exdata_metas);
	struct session_exdata_meta *t_meta;
	for(unsigned int i = 0; i < len; i++)
	{
		t_meta = (struct session_exdata_meta *)utarray_eltptr(global_session_exdata_metas, i);
		if(strcmp(t_meta->name, name) == 0)
		{
			return t_meta->idx;
		}
	}
	struct session_exdata_meta meta;
	meta.free_func=free_func;
	meta.name=name;
	meta.idx=len;
	meta.arg=arg;
	utarray_push_back(global_session_exdata_metas, &meta);
	return meta.idx;
}

int session_set_ex_data(struct session *sess, int idx, void *ex_ptr)
{
	if(sess->ex_data_rt == NULL)return -1;
	if(utarray_len(sess->ex_data_rt) > (unsigned int)idx)return -1;
	struct session_exdata_runtime* exdata = (struct session_exdata_runtime*)utarray_eltptr(sess->ex_data_rt, (unsigned int)idx);
	struct session_exdata_meta* meta = (struct session_exdata_meta*)utarray_eltptr(global_session_exdata_metas, (unsigned int)idx);
	if(exdata)
	{
		exdata->sess=sess;
		if(meta->free_func && exdata->exdata)
		{
			meta->free_func(sess, idx, exdata->exdata, meta->arg);
		}
		exdata->exdata = ex_ptr;
	}
	return -1;
}

void *session_get_ex_data(struct session *sess, int idx)
{
	if(sess->ex_data_rt == NULL)return NULL;
	if(utarray_len(sess->ex_data_rt) > (unsigned int)idx)return NULL;
	struct session_exdata_runtime* exdata = (struct session_exdata_runtime*)utarray_eltptr(sess->ex_data_rt, (unsigned int)idx);
	return exdata->exdata;
}

struct session_mq_node
{
	const char *topic_name;
    void *message;
	size_t message_len;
    STAILQ_ENTRY(session_mq_node) entries; 
};

struct session_mq
{
	STAILQ_HEAD(session_mq_head, session_mq_node) head;
};


struct subscriber 
{
    msg_consume_cb_func *read_cb;
    void *cb_arg;
    STAILQ_ENTRY(subscriber) entries;
};

struct session_mq_topic_schema
{
	char *topic_name;
	msg_free_cb_func *free_cb;
	void *cb_arg;
	STAILQ_HEAD(sub_q_head, subscriber) subscribers; 
	UT_hash_handle hh;
};

static struct session_mq_topic_schema *global_session_mq_schema = NULL;
struct session_mq *session_mq_new(void) 
{
    struct session_mq *mq = CALLOC(struct session_mq, 1);
    STAILQ_INIT(&mq->head);
    return mq;
}

void session_mq_free(struct session_mq *mq)
{
	struct session_mq_node *node;
	while ((node = STAILQ_FIRST(&mq->head)) != NULL)
	{
		STAILQ_REMOVE_HEAD(&mq->head, entries);
		free(node);
	}
	FREE(mq);
}

static struct session_mq_topic_schema *session_mq_topic2schema(const char *topic_name)
{
	struct session_mq_topic_schema *schema;
	HASH_FIND_STR(global_session_mq_schema, topic_name, schema);
	return schema;
};

void session_mq_cleanup()
{
	struct session_mq_topic_schema *current, *tmp;
	HASH_ITER(hh, global_session_mq_schema, current, tmp)
	{
		HASH_DEL(global_session_mq_schema, current); // delete frome global_session_mq_schema
		// free topic_name
		if (current->topic_name)
		{
			FREE(current->topic_name);
		}

		// cleanup STAILQ
		struct subscriber *sub;
		while (!STAILQ_EMPTY(&current->subscribers))
		{
			sub = STAILQ_FIRST(&current->subscribers);
			STAILQ_REMOVE_HEAD(&current->subscribers, entries);
			FREE(sub); // free subscriber
		}
		FREE(current); // free the item
	}
	return;
}

void session_mq_loop(struct session *sess)
{
	struct session_mq_node *node;
	struct subscriber *sub;
	struct session_mq *mq = sess->mq;
	while (!STAILQ_EMPTY(&mq->head))
	{
		node = STAILQ_FIRST(&mq->head);
		STAILQ_REMOVE_HEAD(&mq->head, entries);
		struct session_mq_topic_schema *topic= session_mq_topic2schema(node->topic_name);
		if(topic != NULL)
		{
			STAILQ_FOREACH(sub, &topic->subscribers, entries)
			{
				sub->read_cb(sess, node->topic_name, node->message, sub->cb_arg);
			}
			if(topic->free_cb != NULL)
			{
				topic->free_cb(node->message, topic->cb_arg);
			}
		}
		FREE(node);
	}
	return;
};

int session_mq_create_topic(const char *topic_name, msg_free_cb_func *free_cb, void *cb_arg)
{
	struct session_mq_topic_schema *topic;
	// check if topic_name exist
	HASH_FIND_STR(global_session_mq_schema, topic_name, topic);
	//if found and free_cb is not NULL, it's a exist producer
	if (topic != NULL && topic->free_cb != NULL)
	{
		return -1; // exist
	}
	// if topic not found, create new topic, called by consumer or producer
	if (topic == NULL)
	{
		topic = CALLOC(struct session_mq_topic_schema, 1);
		topic->topic_name = strdup(topic_name); // 复制字符串，确保独立存储
		HASH_ADD_KEYPTR(hh, global_session_mq_schema, topic->topic_name, strlen(topic->topic_name), topic);
		if (STAILQ_EMPTY(&topic->subscribers))
		{
			STAILQ_INIT(&topic->subscribers);
		}
	}
	//if found but free_cb is NULL, it's a consumer's placeholder, now filled by producer
	if (free_cb != NULL)
	{
		topic->free_cb = free_cb;
		topic->cb_arg = cb_arg;
	}
	return 0; // success
}

int session_mq_destroy_topic(const char *topic_name) {
    struct session_mq_topic_schema *topic;

    // find topic in global schma
    HASH_FIND_STR(global_session_mq_schema, topic_name, topic);
    if (topic == NULL) 
	{
        return -1; // not found	
    }
    // delete topic from global schema
    HASH_DEL(global_session_mq_schema, topic);
    // free topic name
    FREE(topic->topic_name);

	struct subscriber *sub;
	while ((sub = STAILQ_FIRST(&topic->subscribers)) != NULL)
	{
		STAILQ_REMOVE_HEAD(&topic->subscribers, entries);
		FREE(sub);
	}
    // free topic itself
    FREE(topic);
    return 0; // success
}


int session_produce_message(struct session *sess, const char *topic_name, void *data)
{
	struct session_mq_topic_schema *topic;
	HASH_FIND_STR(global_session_mq_schema, topic_name, topic);
	if (topic == NULL)
	{
		return -1;  
	}
	struct session_mq *mq=sess->mq;
	struct session_mq_node *node= CALLOC(struct session_mq_node,1);
	node->topic_name = topic->topic_name;
	node->message = data;
	STAILQ_INSERT_TAIL(&mq->head, node, entries);
	return 0;
}

int session_mq_subscribe_topic(const char *topic_name, msg_consume_cb_func *read_cb, void *cb_arg, plugin_id_t self)
{
	struct session_mq_topic_schema *topic;
	session_mq_create_topic(topic_name, NULL, NULL);
    HASH_FIND_STR(global_session_mq_schema, topic_name, topic);
    if (topic == NULL) 
	{
        return -1; // topic create failed
    }
	struct subscriber *new_subscriber = CALLOC(struct subscriber,1);
    new_subscriber->read_cb = read_cb;
    new_subscriber->cb_arg = cb_arg;
	STAILQ_INSERT_TAIL(&topic->subscribers, new_subscriber, entries);
	return 0;
}

int session_mq_unsubscribe_topic(const char *topic_name, msg_consume_cb_func *read_cb, void *cb_arg)
{
	struct session_mq_topic_schema *topic;
	int unsubscribe_count = 0;
	HASH_FIND_STR(global_session_mq_schema, topic_name, topic);
	if (topic == NULL)
	{
		return -1; // opic create failed
	}
	struct subscriber *sub, *tmp_sub;

	sub = STAILQ_FIRST(&topic->subscribers);
	while (sub)
	{
		if (sub->read_cb == read_cb && sub->cb_arg == cb_arg)
		{
			tmp_sub = STAILQ_NEXT(sub, entries);						   
			STAILQ_REMOVE(&topic->subscribers, sub, subscriber, entries); 
			FREE(sub);													  
			unsubscribe_count++;
			sub = tmp_sub;											   
		}
		else
		{
			sub = STAILQ_NEXT(sub, entries); // next sub
		}
	}
	return unsubscribe_count;
}

struct streaminfo;
struct session *session_new(struct streaminfo* stream, enum session_type type, int thread_id)
{
	if(stream == NULL)return NULL;
	if(type!=SESSION_TYPE_TCP && type!=SESSION_TYPE_UDP)return NULL;
	struct session *sess = CALLOC(struct session, 1);
	sess->stream = stream;
	sess->type = type;
	sess->state = SESSION_STATE_OPENING;
	sess->mq = session_mq_new();

	utarray_new(sess->ex_data_rt, &session_exdata_runtime_icd);
	utarray_resize(sess->ex_data_rt, utarray_len(global_session_exdata_metas));
	return sess;

}

void session_free(struct session *sess,  int thread_id)
{
	if(sess==NULL)return;
	if(sess->mq != NULL)
	{
		session_mq_free(sess->mq);
	}
	utarray_free(sess->ex_data_rt);
	FREE(sess);
}

void session_set_state(struct session *session, enum session_state state, int thread_id)
{
	if(session == NULL)return;
	session->state = state;
}