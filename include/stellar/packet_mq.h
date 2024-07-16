#pragma once

#include "stellar.h"

//session mq
typedef void packet_msg_free_cb_func(struct packet *pkt, void *msg, void *msg_free_arg);
typedef void on_packet_msg_cb_func(struct packet *pkt, int topic_id, const void *msg, void *plugin_env);

//return topic_id
int stellar_packet_mq_create_topic(struct stellar *st, const char *topic_name, packet_msg_free_cb_func *msg_free_cb, void *msg_free_arg);

int stellar_packet_mq_get_topic_id(struct stellar *st, const char *topic_name);

int stellar_packet_mq_update_topic(struct stellar *st, int topic_id, packet_msg_free_cb_func *msg_free_cb, void *msg_free_arg);

int stellar_packet_mq_destroy_topic(struct stellar *st, int topic_id);

//return 0 if success, otherwise return -1.
int stellar_packet_mq_subscribe(struct stellar *st, int topic_id, on_packet_msg_cb_func *plugin_on_msg_cb, int plugin_id); //packet plugin only

int packet_mq_publish_message(struct packet *pkt, int topic_id, void *msg);

