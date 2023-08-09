
#include "utils.h"
#include "session_mq_internal.h"

#include <sys/queue.h>
#include "uthash/uthash.h"

struct session_mq_node
{
	const char *topic_name;
    void *message;
	size_t message_len;
    STAILQ_ENTRY(session_mq_node) entries; 
};

struct session_mq
{
	STAILQ_HEAD(, session_mq_node) head;
};


struct subscriber 
{
    session_mq_read_callback *read_cb;
    void *cb_arg;
    STAILQ_ENTRY(subscriber) entries;
};

struct session_mq_topic_schema
{
	const char *topic_name;
	session_mq_free_callback *free_cb;
	void *cb_arg;
	STAILQ_HEAD(, subscriber) subscribers; 
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
}

static struct session_mq_topic_schema *session_mq_topic_schema(const char *topic_name)
{
	struct session_mq_topic_schema *schema;
	HASH_FIND_STR(global_session_mq_schema, topic_name, schema);
	return schema;
};

void session_mq_dequeue(struct session_mq *mq, struct session *session)
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
				sub->read_cb(session, node->topic_name, node->message, node->message_len, sub->cb_arg);
			}
			if(topic->free_cb != NULL)
			{
				topic->free_cb(session, node->topic_name, node->message, node->message_len, topic->cb_arg);
			}
		}
		FREE(node);
	}
	return;
};

int session_mq_topic_create(const char *topic_name, session_mq_free_callback *free_cb, void *cb_arg) {
    struct session_mq_topic_schema *topic;
    // 检查topic_name是否已存在
    HASH_FIND_STR(global_session_mq_schema, topic_name, topic);
    if (topic != NULL && topic->free_cb != NULL)
	{
        return -1; // 已存在
    }
	if(topic == NULL)
	{
		topic = CALLOC(struct session_mq_topic_schema,1);
    	HASH_ADD_KEYPTR(hh, global_session_mq_schema, topic->topic_name, strlen(topic->topic_name), topic);
	}
	if(topic->topic_name)
	{
 	   topic->topic_name = strdup(topic_name); // 复制字符串，确保独立存储
	}
    topic->free_cb = free_cb;
	if(STAILQ_EMPTY(&topic->subscribers))
	{
		STAILQ_INIT(&topic->subscribers);
	}
    return 0; // 成功
}


int session_mq_topic_destroy(const char *topic_name) {
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
    free((void*)topic->topic_name);

	struct subscriber *sub;
	while ((sub = STAILQ_FIRST(&topic->subscribers)) != NULL)
	{
		STAILQ_REMOVE_HEAD(&topic->subscribers, entries);
		free(sub);
	}
    // 释放结构体
    free(topic);
    return 0; // 成功
}


int session_mq_send_msg(struct session *session, const char *topic_name, const void *data, const size_t data_len)
{
	struct session_mq_topic_schema *topic;
	HASH_FIND_STR(global_session_mq_schema, topic_name, topic);
	if (topic == NULL)
	{
		return -1; // 没有找到
	}
	struct session_mq_node *node= CALLOC(struct session_mq_node,1);
	node->topic_name = topic->topic_name;
	node->message = CALLOC(char, data_len);
	memcpy(node->message, data, data_len);
	node->message_len=data_len;
	return 0;
}

int session_mq_topic_subscribe(const char *topic_name, session_mq_read_callback *read_cb, void *cb_arg)
{
	struct session_mq_topic_schema *topic;
	session_mq_topic_create(topic_name, NULL, NULL);
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