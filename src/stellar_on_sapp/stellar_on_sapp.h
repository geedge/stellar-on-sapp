#pragma once

#include "stellar_internal.h"

struct streaminfo;

struct stellar *stellar_init_on_sapp(const char *toml_conf_path);
void stellar_exit_on_sapp(struct stellar *st);

//streaminfo open
struct session *session_new_on_sapp(struct stellar *st, struct streaminfo *stream);
void session_free_on_sapp(struct session *sess);

unsigned char session_state_update_on_sapp(struct streaminfo *stream, unsigned char stream_state, struct session *sess, void *a_packet, enum packet_type type);

void session_poll_on_sapp(struct session *sess);

void packet_update_on_sapp(struct stellar *st, struct streaminfo *pstream, void *a_packet, enum packet_type type);

//return polling work state, 0: idle, 1: working
int polling_on_sapp(struct stellar *st);