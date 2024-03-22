#pragma once

#include <sys/time.h>

struct stellar;

int stellar_get_worker_thread_num(struct stellar *st);
int stellar_get_current_thread_id(struct stellar *st);

int stellar_get_session_mq_topic_cnt(struct stellar *st);
int stellar_get_session_mq_subscriber_cnt(struct stellar *st, int topic_id);


typedef void *plugin_init_callback(struct stellar *st);
typedef void plugin_exit_callback(void *plugin_ctx);

typedef int stellar_periodic_cb_func(struct stellar *st, void *cb_arg);

int stellar_worker_thread_periodic_add(struct stellar *st, stellar_periodic_cb_func *periodic_cb, void *cb_arg, const struct timeval *interval);

typedef int plugin_on_polling_func(void *cb_arg);

//return polling plugin_id
int stellar_polling_plugin_register(struct stellar *st,  plugin_on_polling_func on_polling, void *cb_arg);