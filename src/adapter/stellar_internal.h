#pragma once
#include "stellar/stellar.h"

#include "uthash/utarray.h"

struct stellar
{
	UT_array *intrinsic_session_event_schema_array;
	UT_array *plugin_specs_array;
	UT_array *session_mq_schema_array;
	UT_array *session_exdata_schema_array;
};