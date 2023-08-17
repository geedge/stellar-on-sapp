#pragma once

#include "session.h"
#include "stellar.h"

typedef void session_ex_free(struct session *sess, int idx, void *ex_ptr, void *arg);
int stellar_session_get_ex_new_index(struct stellar *st, const char *name, session_ex_free *free_func,void *arg); 
int session_set_ex_data(struct session *sess, int idx, void *ex_ptr);
void *session_get_ex_data(struct session *sess, int idx);

