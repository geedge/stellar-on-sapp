#ifdef __cplusplus
extern "C"
{
#endif

#include "plugin_manager/plugin_manager_interna.h"
#include "stellar_on_sapp/stellar_internal.h"
#include "stellar/session.h"



//mock stellar
struct stellar
{
	struct plugin_manager_schema *plug_mgr;
};

struct packet
{
	struct stellar *st;
	enum packet_type type;
	unsigned char ip_proto;
};

struct session
{
	struct plugin_manager_runtime *plug_mgr_rt;
	enum session_type type;
	enum session_state state;
	int sess_pkt_cnt;
};
enum session_state session_get_current_state(struct session *sess)
{
    return sess->state;
}

enum session_type session_get_type(struct session *sess)
{
	return sess->type;

}

struct plugin_manager_schema * stellar_plugin_manager_schema_get(struct stellar *st)
{
	return st->plug_mgr;
}

int stellar_plugin_manager_schema_set(struct stellar *st, struct plugin_manager_schema *pm)
{
	st->plug_mgr=pm;
	return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

int stellar_get_worker_thread_num(struct stellar *st)
{
	return 16;
}

int stellar_get_current_thread_id(struct stellar *st)
{
	return 0;
}
#pragma GCC diagnostic pop

struct stellar * packet_stellar_get(struct packet *pkt)
{
	return pkt->st;
}

struct plugin_manager_runtime * session_plugin_manager_runtime_get(struct session *sess)
{
	return sess->plug_mgr_rt;
}

unsigned char packet_get_ip_protocol(struct packet *pkt)
{
	return pkt->ip_proto;
}


enum packet_type packet_get_type(const struct packet *pkt)
{
	return pkt->type;
}


#ifdef __cplusplus
}
#endif