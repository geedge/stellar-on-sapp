#pragma once

struct session;
struct stellar;

//return plugin_env
typedef void *plugin_on_load_func(struct stellar *st);
typedef void plugin_on_unload_func(void *plugin_env);

//return per_session_ctx
typedef void *session_ctx_new_func(struct session *sess, void *plugin_env);
typedef void session_ctx_free_func(struct session *sess, void *session_ctx, void *plugin_env);

#define TOPIC_TCP "TCP"
#define TOPIC_TCP_STREAM "TCP_STREAM"
#define TOPIC_UDP "UDP"
#define TOPIC_EGRESS "EGRESS"
#define TOPIC_CONTROL_PACKET "CONTROL_PACKET"
							
//return session plugin_id
int stellar_session_plugin_register(struct stellar *st, 
									session_ctx_new_func session_ctx_new, 
									session_ctx_free_func session_ctx_free, 
									void *plugin_env);

void stellar_session_plugin_dettach_current_session(struct session *sess);



struct packet;
typedef void *plugin_on_packet_func(struct packet *pkt,  unsigned char ip_protocol, void *plugin_env);

//return packet plugin_id
int stellar_packet_plugin_register(struct stellar *st, unsigned char ip_protocol, plugin_on_packet_func on_packet, void *plugin_env);


