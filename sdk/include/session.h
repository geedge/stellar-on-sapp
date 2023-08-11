#pragma once

#include <stdint.h>
#include <stddef.h>

/* session direction definition */
#define SESSION_DIR_C2S 			0x01
#define SESSION_DIR_S2C 			0x02
#define SESSION_DIR_DOUBLE 			0x03

enum session_type
{
	SESSION_TYPE_CUSTOM,
	SESSION_TYPE_TCP,
	SESSION_TYPE_UDP,
	SESSION_TYPE_MAX,
};

enum session_state
{
	SESSION_STATE_INVALID = 0,
	SESSION_STATE_OPENING = 1 << 1,
	SESSION_STATE_ACTIVE = 1 << 2,
	SESSION_STATE_CLOSING = 1 << 3,
	SESSION_STATE_ALL = (1 << 1 | 1 << 2 | 1 << 3),
};

struct session;
uint8_t session_get_flow_flag(const struct session *sess);        	//  tcp or udp 
uint8_t session_current_direction_get(const struct session *sess);   //  tcp or udp 
const char *session_get_readable_addr(struct session *sess);
const char *session_get_payload(struct session *sess, int *payload_len);

struct packet;
const struct packet *session_get_current_packet(struct session *sess);

typedef plugin_id_t int;
//session mq
typedef void msg_free_cb_func(void *msg, void *cb_arg);
typedef int  msg_consume_cb_func(struct session *sess, const char *topic_name, void *msg, void *cb_arg);

int session_mq_create_topic(const char *topic_name, msg_free_cb_func *free_cb, void *cb_arg);
int session_mq_destroy_topic(const char *topic_name);
int session_produce_message(struct session *sess, const char *topic_name, void *msg);
int session_ignore_message(struct session *sess, const char *topic_name, int consumer_id);

//return consumer_id >=0 if success, otherwise return -1.
int session_mq_subscribe_topic(const char *topic_name, msg_consume_cb_func *read_cb, void *cb_arg, plugin_id_t self);
int session_mq_unsubscribe_topic(const char *topic_name, msg_consume_cb_func *read_cb, void *cb_arg,  plugin_id_t self);


typedef void sess_ex_free(struct session *sess, int idx, void *ex_ptr, void *arg);
int session_get_ex_new_index(void *arg, const char *name, sess_ex_free *free_func);
int session_set_ex_data(struct session *sess, int idx, void *ex_ptr);
void *session_get_ex_data(struct session *sess, int idx);

void session_dettach_plugin(struct session *sess, plugin_id_t self);
void session_attach_plugin(struct session *sess, plugin_id_t self);


//session plugin
void *session_get_plugin_ctx(struct session *sess, int plugin_id);	
void session_set_plugin_ctx(struct session *sess, int plugin_id, void *ctx);
void plugin_dettach_session(struct session *sess,  int plugin_id);
void plugin_attach_session(struct session *sess,  int plugin_id);
