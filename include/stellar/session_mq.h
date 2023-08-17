#pragma once

#include "session.h"
#include "stellar.h"

#include <sys/time.h>

//session mq
typedef void msg_free_cb_func(void *msg, void *cb_arg);
typedef int  on_msg_cb_func(struct session *sess, int topic_id, const void *msg, void *cb_arg);

//return topic_id
int session_mq_create_topic(struct stellar *st, const char *topic_name, msg_free_cb_func *free_cb, void *cb_arg);
int session_mq_get_topic_id(struct stellar *st, const char *topic_name);

int session_mq_update_topic(struct stellar *st, int topic_id, msg_free_cb_func *free_cb, void *cb_arg);

int session_mq_destroy_topic(struct stellar *st, int topic_id);

//return sub_id >=0 if success, otherwise return -1.
int session_mq_subscribe_topic(struct stellar *st, int topic_id, on_msg_cb_func *sub_cb, void *cb_arg);

int session_mq_publish_message(struct session *sess, int topic_id, void *msg);
int session_mq_ignore_message(struct session *sess, int topic_id, int sub_id);