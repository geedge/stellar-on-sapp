#include "sdk/include/session.h"
#include "sdk/include/session_mq.h"
#include "sdk/include/subsystem.h"

struct adapter;
struct adapter *adapter_init(void);
void adapter_exit(struct adapter *adapter);

struct session *session_new(void);
//entry
void session_free(struct session *session);
