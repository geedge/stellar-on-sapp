#pragma once

#include "stellar.h"

typedef void session_exdata_free(struct session *sess, int idx, void *ex_ptr, void *arg);
int stellar_session_exdata_new_index(struct stellar *st, const char *name, session_exdata_free *free_func,void *arg); 
int session_exdata_set(struct session *sess, int idx, void *ex_ptr);
void *session_exdata_get(struct session *sess, int idx);