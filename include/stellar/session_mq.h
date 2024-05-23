#pragma once

#include "stellar.h"

//session mq
typedef void session_msg_free_cb_func(void *msg, void *msg_free_arg);
typedef void on_session_msg_cb_func(struct session *sess, int topic_id, const void *msg, void *per_session_ctx, void *plugin_env);

//return topic_id
int stellar_session_mq_create_topic(struct stellar *st, const char *topic_name, session_msg_free_cb_func *msg_free_cb, void *msg_free_arg);

int stellar_session_mq_get_topic_id(struct stellar *st, const char *topic_name);

int stellar_session_mq_update_topic(struct stellar *st, int topic_id, session_msg_free_cb_func *msg_free_cb, void *msg_free_arg);

int stellar_session_mq_destroy_topic(struct stellar *st, int topic_id);

//return 0 if success, otherwise return -1.
int stellar_session_mq_subscribe(struct stellar *st, int topic_id, on_session_msg_cb_func *plugin_on_msg_cb, int plugin_id);

int session_mq_publish_message(struct session *sess, int topic_id, void *msg);

int session_mq_ignore_message(struct session *sess, int topic_id, int plugin_id);
int session_mq_unignore_message(struct session *sess, int topic_id, int plugin_id);
