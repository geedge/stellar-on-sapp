#pragma once

#include "stellar/session.h"

#include "stellar/stellar.h"
#include "uthash/utarray.h"

struct session_exdata_runtime;
struct session_mq;
struct packet;

typedef void session_packet_free(struct session *sess, struct packet *pkt, void *arg);

struct session
{
	enum session_type type;
	enum session_state state;
	struct packet *cur_pkt;
	session_packet_free *cur_pkt_free_cb;
	void *cur_pkt_free_arg;
	struct stellar *st;
	struct session_event  *intrinsic_events;
	struct session_exdata_runtime *ex_data_rt;
	struct session_mq *mq;
	struct session_event *ev_list;
	uint64_t set_detach_others;
};

struct session *session_new(struct stellar *st, enum session_type type, int thread_id);

struct packet *session_get_current_packet(struct session *sess);
void session_set_current_packet(struct session *sess, struct packet *pkt, session_packet_free *free_cb, void *arg);

void session_free(struct session *sess);

void session_defer_loop(struct session *sess);

void session_dispatch(struct session *sess, enum session_state state, struct packet *pkt);