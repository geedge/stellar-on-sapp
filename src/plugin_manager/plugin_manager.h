#pragma once

#include "stellar/stellar.h"

struct plugin_manager_schema;
struct plugin_manager_runtime;

struct plugin_manager_schema *plugin_manager_init(struct stellar *st, const char *plugin_spec_file_path);
void plugin_manager_exit(struct plugin_manager_schema *plug_mgr);

void plugin_manager_on_packet_ingress(struct plugin_manager_schema *plug_mgr, struct packet *pkt);
void plugin_manager_on_packet_egress(struct plugin_manager_schema *plug_mgr, struct packet *pkt);

//return polling work state, 0: idle, 1: working
int plugin_manager_on_polling(struct plugin_manager_schema *plug_mgr);

//publish and dispatch session msg(msg, pkt) on session_mq
void plugin_manager_on_session_ingress(struct session *sess,struct packet *pkt);
void plugin_manager_on_session_egress(struct session *sess,struct packet *pkt);

struct plugin_manager_runtime *plugin_manager_session_runtime_new(struct plugin_manager_schema *plug_mgr, struct session *sess);
void plugin_manager_session_runtime_free(struct plugin_manager_runtime *plug_mgr_rt);