#pragma once

#include <stdint.h>
#include <stddef.h>

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
	__SESSION_STATE_MAX,
};

struct flow_flag 
{
    unsigned char flow_flag_see_client_to_server:1;
    unsigned char flow_flag_see_server_to_client:1;
    unsigned char flow_flag_client_is_local:1;
    unsigned char flow_flag_server_is_local:1;
	unsigned char pad:4;
};
	
struct session;
struct flow_flag  session_get_flow_flag(const struct session *sess);       	
struct flow_flag  session_get_current_flow_flag(struct session *sess);   	
const char *session_get_readable_addr(struct session *sess);
const char *session_get_current_payload(struct session *sess, size_t *payload_len);

struct packet;
const struct packet *session_get_current_packet(struct session *sess);
const char *packet_get_data(const struct packet *pkt, size_t *data_len);

typedef  int plugin_id_t;
//session mq
typedef void msg_free_cb_func(void *msg, void *cb_arg);
typedef int  msg_subscriber_cb_func(struct session *sess, const char *topic_name, const void *msg, void *cb_arg);

int session_mq_create_topic(const char *topic_name, msg_free_cb_func *free_cb, void *cb_arg);
int session_mq_destroy_topic(const char *topic_name);
int session_publish_message(struct session *sess, const char *topic_name, void *msg);
int session_ignore_message(struct session *sess, const char *topic_name, int sub_id);

//return sub_id >=0 if success, otherwise return -1.
int session_mq_subscribe_topic(const char *topic_name, msg_subscriber_cb_func *sub_cb, void *cb_arg);


typedef void session_ex_free(struct session *sess, int idx, void *ex_ptr, void *arg);
int session_get_ex_new_index(const char *name, session_ex_free *free_func,void *arg); // Only support in INIT stage
int session_set_ex_data(struct session *sess, int idx, void *ex_ptr);
void *session_get_ex_data(struct session *sess, int idx);

void session_dettach_plugin(struct session *sess, plugin_id_t self);
void session_attach_plugin(struct session *sess, plugin_id_t self);

