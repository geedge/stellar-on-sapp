#pragma once

#include <stddef.h>

struct session;

typedef void session_mq_free_callback(const struct session *session, const char* topic_name, void *data, const size_t data_len, void *cb_arg);
typedef int  session_mq_read_callback(const struct session *session, const char* topic_name, void *data, const size_t data_len, void *cb_arg);

int session_mq_topic_create(const char *topic_name, session_mq_free_callback *free_cb, void *cb_arg);

int session_mq_topic_destroy(const char *topic_name);

int session_mq_send_msg(struct session *session, const char *topic, const void *data, const size_t data_len);

int session_mq_topic_subscribe(const char *topic_name, session_mq_read_callback *read_cb, void *cb_arg);