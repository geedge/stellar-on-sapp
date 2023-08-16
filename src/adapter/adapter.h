#include "session.h"
#include "plugin_spec.h"

#include <stdint.h>

struct plugin_specific
{
	plugin_init_callback *init_cb;
	plugin_exit_callback *exit_cb;
	plugin_session_callback *udp_session_entry_cb;
	plugin_session_callback *tcp_session_entry_cb;
	int udp_session_interest_state;
	int tcp_session_interest_state;
	
};

int adapter_init(struct plugin_specific specs[] , int specs_num);
void adapter_exit();




struct streaminfo;

//streaminfo open
void adapter_session_open(struct streaminfo *stream, struct session **session, int thread_id);
void adapter_session_active(struct streaminfo *stream, struct session **session, int thread_id);

//streaminfo close, or firewall active close streaminfo  
void adapter_session_close(struct streaminfo *stream, struct session **session, int thread_id);