#pragma once

#include <sys/time.h>

struct stellar;

int stellar_get_worker_thread_num(struct stellar *st);
int stellar_get_current_thread_id(struct stellar *st);

typedef void *plugin_init_callback(struct stellar *st);
typedef void plugin_exit_callback(void *plugin_ctx);

typedef int stellar_periodic_cb_func(struct stellar *st, void *cb_arg);

int stellar_worker_thread_periodic_add(struct stellar *st, stellar_periodic_cb_func *periodic_cb, void *cb_arg, const struct timeval *interval);

struct plugin_specific
{
	plugin_init_callback *init_cb;
	plugin_exit_callback *exit_cb;
};

struct stellar *stellar_init(struct plugin_specific specs[] , int specs_num);
void stellar_exit(struct stellar *);