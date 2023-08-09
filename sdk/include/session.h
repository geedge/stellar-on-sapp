#pragma once

#include <stdint.h>

/* session direction definition */
#define SESSION_DIR_C2S 			0x01
#define SESSION_DIR_S2C 			0x02
#define SESSION_DIR_DOUBLE 			0x03

enum session_type
{
	SESSION_TYPE_CUSTOM,
	SESSION_TYPE_TCP,
	SESSION_TYPE_UDP,
	SESSION_TYPE_MAX,
};

enum session_state
{
	SESSION_STATE_INVALID = 0,
	SESSION_STATE_OPENING = 1 << 1,
	SESSION_STATE_ACTIVE = 1 << 2,
	SESSION_STATE_CLOSING = 1 << 3,
	SESSION_STATE_ALL = (1 << 1 | 1 << 2 | 1 << 3),
};

struct session;
uint8_t session_direction_get(struct session *session);        	//  tcp or udp 
uint8_t session_current_direction_get(struct session *session);   //  tcp or udp 



