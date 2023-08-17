#pragma once

#include "stellar/session.h"

struct streaminfo;

//streaminfo open
struct session *adapter_session_open(struct stellar *st, struct streaminfo *stream, void *a_packet);
void adapter_session_active(struct streaminfo *stream, struct session *session, void *a_packet);

//streaminfo close, or firewall active close streaminfo  
void adapter_session_close(struct streaminfo *stream, struct session *session, void *a_packet);

void adapter_session_poll(struct session *sess);