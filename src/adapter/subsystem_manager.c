#include "utils.h"
#include "session.h"
#include "session_manager.h"
#include "subsystem_manager.h"

struct subsystem_schema *subsystem_get0_schema(enum session_type type, subsystem_entry_callback *entry_cb)
{
	int subsystem_num = sizeof(global_subsystem_schema) / sizeof(global_subsystem_schema[0]);
	struct subsystem_schema *schema = NULL;
	for(int i = 0; i < subsystem_num; i++)
	{
		if(type==SESSION_TYPE_TCP)
		{
			if(global_subsystem_schema[i].tcp_session_entry_cb == entry_cb)
			{
				schema = &global_subsystem_schema[i];
				break;
			}
		}
		if(type==SESSION_TYPE_UDP)
		{
			if(global_subsystem_schema[i].udp_session_entry_cb == entry_cb)
			{
				schema = &global_subsystem_schema[i];
				break;
			}
		}
	}
	return schema;
}

static int subsystem_session_rt_index(struct subsystem_schema *schema)
{
	size_t array_size = sizeof(global_subsystem_schema) / sizeof(global_subsystem_schema[0]);
	size_t struct_size = sizeof(struct subsystem_schema);
	struct subsystem_schema* array_start = global_subsystem_schema;
	int index = -1;
	// 计算下标
	ptrdiff_t diff = (char *)schema - (char *)array_start;
	if (diff >= 0 && diff % struct_size == 0 && diff / struct_size < array_size)
	{
		index = (int)(diff / struct_size);
	}
	return index;
}

void *subsystem_session_ctx_get(struct session *session, struct subsystem_schema *schema)
{
	int index=subsystem_session_rt_index(schema);
	if(index >= 0 && index < session->subsystem_rt_num)
	{
		return session->subsystem_rt[index].ctx;
	}
	return NULL;
};

void subsystem_dettach_session(struct session *session, struct subsystem_schema *schema)
{
	int index=subsystem_session_rt_index(schema);
	if(index >= 0 && index < session->subsystem_rt_num)
	{
		session->subsystem_rt[index].interest_state = SESSION_STATE_INVALID;
	}
	return;
};

void subsystem_attach_session(struct session *session,  struct subsystem_schema *schema)
{
	int index=subsystem_session_rt_index(schema);
	if(index >= 0 && index < session->subsystem_rt_num)
	{
		session->subsystem_rt[index].interest_state = SESSION_STATE_ALL;
	}
	return;
};