#pragma once

#include "plugin_spec.h"

int test_entry_plugin_init(int plugin_id);
void test_entry_plugin_exit(void);
void test_entry_plugin_entry(struct session *session, enum session_state state, int thread_id);


int test_mq_plugin_init(int plugin_id);
void test_mq_plugin_exit(void);
void test_mq_plugin_entry(struct session *session, enum session_state state, int thread_id);


