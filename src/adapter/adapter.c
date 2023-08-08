#include "sdk/include/session.h"
#include "adapter.h"

struct session
{

};

struct subsystem_descriptor
{

};

struct subsystem_descriptor *subsystem_get0_descriptor(enum session_type type, subsystem_entry_callback *entry_cb)
{
	return NULL;
}
void *subsystem_get_session_ctx(struct session *session, struct subsystem_descriptor *desc)
{
	return NULL;
};

void subsystem_dettach_session(struct session *session,  struct subsystem_descriptor *desc)
{
	return;
};