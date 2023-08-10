#pragma once

#include "plugin_spec.h"

int test_plugin_init(int plugin_id);
void test_plugin_exit(void);
void test_plugin_entry(struct session *session, enum session_state state, int thread_id);