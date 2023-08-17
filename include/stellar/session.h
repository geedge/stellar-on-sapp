#pragma once

#include <stdint.h>
#include <stddef.h>

#include "stellar.h"

enum session_type
{
	SESSION_TYPE_TCP,
	SESSION_TYPE_UDP,
	__SESSION_TYPE_MAX,
};

enum session_state
{
	SESSION_STATE_INVALID = 0,
	SESSION_STATE_OPENING = 1 ,
	SESSION_STATE_ACTIVE = 2,
	SESSION_STATE_CLOSING = 3,
	__SESSION_STATE_MAX,
};


struct session;

#define SESSION_SEEN_C2S_FLOW (1 >> 1)
#define SESSION_SEEN_S2C_FLOW (1 >> 2)
int session_is_symmetric(const struct session *sess, unsigned char *flag);

#define SESSION_DIRECTION_IN 0
#define SESSION_DIRECTION_OUT 1
int  session_get_direction(const struct session *sess);       	

const char *session_get0_readable_addr(struct session *sess);
const char *session_get0_current_payload(struct session *sess, size_t *payload_len);

struct packet;
const struct packet *session_get0_current_packet(struct session *sess);

#define PACKET_DIRECTION_C2S 0
#define PACKET_DIRECTION_S2C 1
int packet_get_direction(const struct packet *pkt);   	
const char *packet_get0_data(const struct packet *pkt, size_t *data_len);

#define SESS_EV_OPENING 1<<1
#define SESS_EV_PACKET 1<<2
#define SESS_EV_CLOSING 1<<3

#define SESS_EV_TCP 1<<4
#define SESS_EV_UDP 1<<5

typedef int session_event_cb_func(struct session *sess, int events, const struct packet *pkt, void *cb_arg);

struct session_event;
//return plugin_id
int stellar_plugin_register(struct stellar *st, int events, session_event_cb_func *cb, void *cb_arg);// register intrinsic event
struct session_event *session_get_intrinsic_event(struct session *sess, int plugin_id);

#include <sys/time.h>

struct session_event *session_event_new(struct stellar *st, struct session *sess, int events, session_event_cb_func *cb, void *cb_arg);
int session_event_add(struct session_event *ev, const struct timeval *timeout);
int session_event_del(struct session_event *ev);
int session_event_assign(struct session_event *ev, struct stellar *st, struct session *sess, int events, session_event_cb_func *cb, void *cb_arg);
void session_event_free(struct session_event *ev);