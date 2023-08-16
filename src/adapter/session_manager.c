#include "utils.h"
#include "session_manager.h"

#include "uthash/uthash.h"
#include "uthash/utarray.h"
#include <sys/queue.h>

struct session_exdata_meta
{
	char *name;
	session_ex_free *free_func;
	void *arg;
	int idx;
};

static void session_exdata_met_copy(void *_dst, const void *_src) 
{
  struct session_exdata_meta *dst = (struct session_exdata_meta*)_dst, *src = (struct session_exdata_meta*)_src;
  dst->free_func = src->free_func;
  dst->arg = src->arg;
  dst->idx = src->idx;
  dst->name = src->name ? strdup(src->name) : NULL;
}

static void session_exdata_met_dtor(void *_elt) 
{
  struct session_exdata_meta *elt = (struct session_exdata_meta*)_elt;
  if (elt->name) FREE(elt->name);
}

UT_icd session_exdata_meta_icd = {sizeof(struct session_exdata_meta), NULL, session_exdata_met_copy, session_exdata_met_dtor};
UT_array *global_session_exdata_metas = NULL;

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
	meta.name=(char *)name;
	meta.idx=len;
	meta.arg=arg;
	utarray_push_back(global_session_exdata_metas, &meta);
	return meta.idx;
}

struct session_exdata_runtime
{
	void *data;
};

int session_set_ex_data(struct session *sess, int idx, void *ex_ptr)
{
	if(sess->ex_data_rt == NULL)return -1;
	unsigned int len=utarray_len(global_session_exdata_metas);
	if(len < (unsigned int)idx)return -1;
	struct session_exdata_runtime* exdata = (struct session_exdata_runtime*)(sess->ex_data_rt+idx);
	struct session_exdata_meta* meta = (struct session_exdata_meta*)utarray_eltptr(global_session_exdata_metas, (unsigned int)idx);
	if(exdata)
	{
		if(meta->free_func && exdata->data)
		{
			meta->free_func(sess, idx, exdata->data, meta->arg);
		}
		exdata->data = ex_ptr;
	}
	return -1;
}

void *session_get_ex_data(struct session *sess, int idx)
{
	if(sess->ex_data_rt == NULL)return NULL;
	unsigned int len = utarray_len(global_session_exdata_metas);
	if(len < (unsigned int)idx)return NULL;
	struct session_exdata_runtime* exdata = (struct session_exdata_runtime*)(sess->ex_data_rt+idx);
	return exdata->data;
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
    on_msg_cb_func *sub_cb;
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
		FREE(node);
	}
	FREE(mq);
}

static struct session_mq_topic_schema *session_mq_topic2schema(struct session_mq_topic_schema *topic_schema, const char *topic_name)
{
	struct session_mq_topic_schema *schema;
	HASH_FIND_STR(topic_schema, topic_name, schema);
	return schema;
};

static void session_mq_cleanup(struct session_mq_topic_schema *schema)
{
	struct session_mq_topic_schema *current, *tmp;
	HASH_ITER(hh, schema, current, tmp)
	{
		HASH_DEL(schema, current); // delete frome global_session_mq_schema
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

void session_manager_cleanup()
{
	session_mq_cleanup(global_session_mq_schema);
	utarray_free(global_session_exdata_metas);
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
		struct session_mq_topic_schema *topic= session_mq_topic2schema(global_session_mq_schema, node->topic_name);
		if(topic != NULL)
		{
			STAILQ_FOREACH(sub, &topic->subscribers, entries)
			{
				sub->sub_cb(sess, node->topic_name, node->message, sub->cb_arg);
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


int session_publish_message(struct session *sess, const char *topic_name, void *data)
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

int session_mq_subscribe_topic(const char *topic_name, on_msg_cb_func *sub_cb, void *cb_arg)
{
	struct session_mq_topic_schema *topic;
	session_mq_create_topic(topic_name, NULL, NULL);
    HASH_FIND_STR(global_session_mq_schema, topic_name, topic);
    if (topic == NULL) 
	{
        return -1; // topic create failed
    }
	struct subscriber *new_subscriber = CALLOC(struct subscriber,1);
    new_subscriber->sub_cb = sub_cb;
    new_subscriber->cb_arg = cb_arg;
	STAILQ_INSERT_TAIL(&topic->subscribers, new_subscriber, entries);
	return 0;
}
/*
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
*/

static struct session_exdata_runtime  *session_new_exdata_rt()
{
	struct session_exdata_runtime *exdata_rt = NULL;
	unsigned int len = utarray_len(global_session_exdata_metas);
	if(len > 0)
	{
		exdata_rt=CALLOC(struct session_exdata_runtime, len);
	}
	return exdata_rt;
}

static void session_free_exdata_rt(struct session* sess, struct session_exdata_runtime *exdata_rt)
{
	if(exdata_rt==NULL)return;
	unsigned int len=utarray_len(global_session_exdata_metas);
	for (unsigned int i = 0; i < len; i++)
	{
		struct session_exdata_runtime *exdata = (struct session_exdata_runtime *)(exdata_rt + i);
		struct session_exdata_meta *meta = (struct session_exdata_meta *)utarray_eltptr(global_session_exdata_metas, i);
		if (exdata)
		{
			if (meta->free_func && exdata->data)
			{
				meta->free_func(sess, i, exdata->data, meta->arg);
			}
		}
	}
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
	sess->ex_data_rt = session_new_exdata_rt();
	return sess;

}

void session_free(struct session *sess,  int thread_id)
{
	if(sess==NULL)return;
	if(sess->mq != NULL)
	{
		session_mq_free(sess->mq);
	}
	session_free_exdata_rt(sess, sess->ex_data_rt);
	FREE(sess->ex_data_rt);
	FREE(sess);
}

void session_set_state(struct session *session, enum session_state state, int thread_id)
{
	if(session == NULL)return;
	session->state = state;
}