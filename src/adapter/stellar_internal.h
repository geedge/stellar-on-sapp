#pragma once
#include "stellar/stellar.h"

#include "uthash/utarray.h"

struct stellar
{
	UT_array *intrinsic_session_event_schema_array;
	UT_array *plugin_specs_array;
	UT_array *session_mq_schema_array;
	UT_array *session_exdata_schema_array;

	UT_array *registered_packet_plugin_array;
	UT_array *registered_polling_plugin_array;

	int topic_num;
	int sub_num;
};