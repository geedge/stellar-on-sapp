#include "utils.h"
#include "session_manager.h"

#include "uthash/uthash.h"
#include <sys/queue.h>

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

static struct session_mq_topic_schema *session_mq_topic_schema(const char *topic_name)
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
		HASH_DEL(global_session_mq_schema, current); // 从哈希表中删除
		// 释放其他你需要清理的内存，比如：
		if (current->topic_name)
		{
			FREE(current->topic_name);
		}	

        // 清空STAILQ
        struct subscriber *sub;
        while (!STAILQ_EMPTY(&current->subscribers)) {
            sub = STAILQ_FIRST(&current->subscribers);
            STAILQ_REMOVE_HEAD(&current->subscribers, entries);
            FREE(sub); // 假设你需要释放订阅者
        }
		FREE(current); // 释放当前结构体
	}
	return;
}

void session_mq_loop(struct session_mq *mq, struct session *session)
{
	struct session_mq_node *node;
	struct subscriber *sub;
	while (!STAILQ_EMPTY(&mq->head))
	{
		node = STAILQ_FIRST(&mq->head);
		STAILQ_REMOVE_HEAD(&mq->head, entries);
		struct session_mq_topic_schema *topic= session_mq_topic_schema(node->topic_name);
		if(topic != NULL)
		{
			STAILQ_FOREACH(sub, &topic->subscribers, entries)
			{
				sub->read_cb(session, node->topic_name, node->message, sub->cb_arg);
			}
			if(topic->free_cb != NULL)
			{
				topic->free_cb(session, node->topic_name, node->message, topic->cb_arg);
			}
		}
		FREE(node);
	}
	return;
};

int session_mq_create_topic(const char *topic_name, msg_free_cb_func *free_cb, void *cb_arg)
{
	struct session_mq_topic_schema *topic;
	// 检查topic_name是否已存在
	HASH_FIND_STR(global_session_mq_schema, topic_name, topic);
	// 如果找到现有主题，并且free_cb也不为空，则认为生产者已创建此主题
	if (topic != NULL && topic->free_cb != NULL)
	{
		return -1; // 已存在
	}
	// 如果主题不存在，则创建新主题，由消费者或生产者调用
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
	// 如果找到现有主题但free_cb为空，则为消费者占的坑，现在由生产者来填充
	if (free_cb != NULL)
	{
		topic->free_cb = free_cb;
		topic->cb_arg = cb_arg;
	}
	return 0; // 成功
}

int session_mq_destroy_topic(const char *topic_name) {
    struct session_mq_topic_schema *topic;

    // 在哈希表中查找指定的主题名
    HASH_FIND_STR(global_session_mq_schema, topic_name, topic);
    if (topic == NULL) 
	{
        return -1; // 没有找到
    }
    // 从哈希表中删除
    HASH_DEL(global_session_mq_schema, topic);
    // 释放存储的主题名
    FREE(topic->topic_name);

	struct subscriber *sub;
	while ((sub = STAILQ_FIRST(&topic->subscribers)) != NULL)
	{
		STAILQ_REMOVE_HEAD(&topic->subscribers, entries);
		FREE(sub);
	}
    // 释放结构体
    FREE(topic);
    return 0; // 成功
}


int session_send_message(struct session *session, const char *topic_name, void *data)
{
	struct session_mq_topic_schema *topic;
	HASH_FIND_STR(global_session_mq_schema, topic_name, topic);
	if (topic == NULL)
	{
		return -1;  
	}
	struct session_mq *mq=session->mq;
	struct session_mq_node *node= CALLOC(struct session_mq_node,1);
	node->topic_name = topic->topic_name;
	node->message = data;
	STAILQ_INSERT_TAIL(&mq->head, node, entries);
	return 0;
}

int session_mq_subscribe_topic(const char *topic_name, msg_consume_cb_func *read_cb, void *cb_arg)
{
	struct session_mq_topic_schema *topic;
	session_mq_create_topic(topic_name, NULL, NULL);
    HASH_FIND_STR(global_session_mq_schema, topic_name, topic);
    if (topic == NULL) 
	{
        return -1; // topic 创建失败
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
		return -1; // topic 创建失败
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
			sub = STAILQ_NEXT(sub, entries); // 转到下一个元素
		}
	}
	return unsubscribe_count;
}

struct streaminfo;
struct session *session_new(struct streaminfo* stream, struct plugin_spec *specs, int desc_num, enum session_type type, int thread_id)
{
	if(stream == NULL)return NULL;
	if(type!=SESSION_TYPE_TCP && type!=SESSION_TYPE_UDP)return NULL;
	struct session *session = CALLOC(struct session, 1);
	session->stream = stream;
	session->desc = specs;
	session->type = type;
	session->state = SESSION_STATE_OPENING;
	session->plugin_rt_num = desc_num;
	session->plugin_rt = CALLOC(struct session_runtime, desc_num);
	session->mq = session_mq_new();
	for (int i = 0; i < session->plugin_rt_num; i++)
	{
		if (type == SESSION_TYPE_TCP)
		{
			session->plugin_rt[i].interest_state = specs[i].tcp_session_interest_state;
			session->plugin_rt[i].entry_cb = specs[i].tcp_session_entry_cb;
		}
		if (type == SESSION_TYPE_UDP)
		{
			session->plugin_rt[i].interest_state = specs[i].udp_session_interest_state;
			session->plugin_rt[i].entry_cb = specs[i].udp_session_entry_cb;
		}
	}
	return session;

}

void session_free(struct session *session,  int thread_id)
{
	if(session==NULL)return;
	
	if(session->plugin_rt != NULL)
	{
		FREE(session->plugin_rt);
	}
	if(session->mq != NULL)
	{
		session_mq_free(session->mq);
	}
	FREE(session);
}

void session_dispatch(struct session *session, enum session_state state, int thread_id)
{
	if(session==NULL)return;

	int interest_state=0;
	plugin_entry_callback *entry_cb=NULL;
	session->state=state;	
	for (int i = 0; i < session->plugin_rt_num; i++)
	{
		switch (state)
		{
		//sync from session runtime
		case SESSION_STATE_OPENING:
		case SESSION_STATE_ACTIVE:
		case SESSION_STATE_CLOSING:
			interest_state = session->plugin_rt[i].interest_state;
			entry_cb = session->plugin_rt[i].entry_cb;
			break;

		default:
			break;
		}
		if ((interest_state & session->state) && (entry_cb != NULL))
		{
			entry_cb(session, session->state, thread_id);
		}
	}
	session_mq_loop(session->mq, session);
	return;
}

void *session_get_plugin_ctx(struct session *session, int plugin_id)
{
	if(plugin_id >= 0 && plugin_id < session->plugin_rt_num)
	{
		return session->plugin_rt[plugin_id].ctx;
	}
	return NULL;
};

void session_set_plugin_ctx(struct session *session, int plugin_id, void *ctx)
{
	if(plugin_id >= 0 && plugin_id < session->plugin_rt_num)
	{
		session->plugin_rt[plugin_id].ctx=ctx;
	}
	return;
};

void plugin_dettach_session(struct session *session,int plugin_id)
{
	if(plugin_id >= 0 && plugin_id < session->plugin_rt_num)
	{
		session->plugin_rt[plugin_id].interest_state = SESSION_STATE_INVALID;
	}
	return;
};

void plugin_attach_session(struct session *session,  int plugin_id)
{
	if(plugin_id >= 0 && plugin_id < session->plugin_rt_num)
	{
		session->plugin_rt[plugin_id].interest_state = SESSION_STATE_ALL;
	}
	return;
};