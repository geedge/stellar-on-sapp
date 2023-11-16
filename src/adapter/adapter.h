#pragma once

#include "stellar/session.h"

struct streaminfo;


struct stellar *stellar_init(const char *toml_conf_path);
void stellar_exit(struct stellar *st);

//streaminfo open
struct session *adapter_session_new(struct stellar *st, struct streaminfo *stream);
void adapter_session_free(struct session *sess);

void adapter_session_state_update(struct streaminfo *stream, struct session *sess, void *a_packet, enum session_state state);

void adapter_session_poll(struct session *sess);