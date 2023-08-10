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
uint8_t session_direction_get(struct session *sess);        	//  tcp or udp 
uint8_t session_current_direction_get(struct session *sess);   //  tcp or udp 
const char* session_printaddr(struct session *sess);
const char *session_payload_get(struct session *sess, int *payload_len);

struct packet;
const struct packet *session_packet_get(struct session *sess);


//session mq
typedef void session_mq_free_callback(struct session *sess, const char* topic_name, void *data, void *cb_arg);
typedef int  session_mq_read_callback(struct session *sess, const char* topic_name, void *data, void *cb_arg);

int session_mq_topic_create(const char *topic_name, session_mq_free_callback *free_cb, void *cb_arg);
int session_mq_topic_destroy(const char *topic_name);
int session_mq_send_msg(struct session *sess, const char *topic, void *data);
int session_mq_topic_subscribe(const char *topic_name, session_mq_read_callback *read_cb, void *cb_arg);
int session_mq_topic_unsubscribe(const char *topic_name, session_mq_read_callback *read_cb, void *cb_arg);

//session plugin
void *session_get_plugin_ctx(struct session *session, int plugin_id);	
void session_set_plugin_ctx(struct session *session, int plugin_id, void *ctx);
void plugin_dettach_session(struct session *session,  int plugin_id);
void plugin_attach_session(struct session *session,  int plugin_id);
