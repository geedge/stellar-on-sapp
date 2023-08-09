#pragma once

#include "utils.h"
#include "subsystem.h"

struct subsystem_descriptor
{
	enum session_type type;
	int interest_state;
	subsystem_init_callback *init_cb;
	subsystem_exit_callback *exit_cb;
	subsystem_entry_callback *entry_cb;
};

struct subsystem_descriptor global_subsystem_schema[] = 
{
	{
		.type=SESSION_TYPE_UDP,
		.init_cb=NULL,
		.exit_cb=NULL,
		.entry_cb=NULL,
		.interest_state=SESSION_STATE_ALL,
	},
	{
		.type=SESSION_TYPE_UDP,
		.init_cb=NULL,
		.exit_cb=NULL,
		.entry_cb=NULL,
		.interest_state=SESSION_STATE_ALL,
	},
};