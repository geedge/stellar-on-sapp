#pragma once

#include "stellar/stellar.h"
#include "stellar/session.h"

void *simple_stellar_event_plugin_init(struct stellar *st);
void simple_stellar_event_plugin_exit(void *plugin_ctx);

void *simple_stellar_mq_plugin_init(struct stellar *st);
void simple_stellar_mq_plugin_exit(void *plugin_ctx);

