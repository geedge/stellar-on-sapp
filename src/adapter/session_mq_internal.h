#include "session_mq.h"
#include "session_manager.h"

struct session_mq;

struct session_mq *session_mq_new();
void session_mq_free(struct session_mq *mq);
void session_mq_dequeue(struct session_mq *mq, struct session *session);