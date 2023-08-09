#include "sdk/include/session.h"
#include "sdk/include/session_mq.h"
#include "sdk/include/subsystem.h"

#include <stdint.h>

struct adapter;
struct adapter *adapter_init(void);
void adapter_exit(struct adapter *adapter);


struct streaminfo;

uint8_t adapter_session_open(struct streaminfo *stream, struct session **session, int thread_id);
uint8_t adapter_session_active(struct streaminfo *stream, struct session **session, int thread_id);
uint8_t adapter_session_close(struct streaminfo *stream, struct session **session, int thread_id);