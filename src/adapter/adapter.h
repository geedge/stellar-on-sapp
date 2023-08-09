#include "session.h"
#include "session_mq.h"
#include "subsystem_manager.h"

#include <stdint.h>

struct adapter;
struct adapter *adapter_init(void);
void adapter_exit(struct adapter *adapter);


struct streaminfo;

//streaminfo open
uint8_t adapter_session_open(struct adapter *adapter, struct streaminfo *stream, struct session **session, int thread_id);
uint8_t adapter_session_active(struct adapter *adapter, struct streaminfo *stream, struct session **session, int thread_id);

//streaminfo close, or firewall active close streaminfo  
uint8_t adapter_session_close(struct adapter *adapter, struct streaminfo *stream, struct session **session, int thread_id);