#pragma once

#include "utils.h"
#include "subsystem.h"

struct subsystem_descriptor
{
	subsystem_init_callback *init_cb;
	subsystem_exit_callback *exit_cb;
	subsystem_entry_callback *udp_session_entry_cb;
	subsystem_entry_callback *tcp_session_entry_cb;
	int udp_session_interest_state;
	int tcp_session_interest_state;
};

struct subsystem_descriptor global_subsystem_descriptor[] = 
{
	{
		.init_cb=NULL,
		.exit_cb=NULL,
		.udp_session_entry_cb=NULL,
		.udp_session_interest_state=SESSION_STATE_ALL,
		.tcp_session_entry_cb=NULL,
		.tcp_session_interest_state=SESSION_STATE_ALL,
	},
	{
		.init_cb=NULL,
		.exit_cb=NULL,
		.udp_session_entry_cb=NULL,
		.udp_session_interest_state=SESSION_STATE_ALL,
		.tcp_session_entry_cb=NULL,
		.tcp_session_interest_state=SESSION_STATE_ALL,
	},
};