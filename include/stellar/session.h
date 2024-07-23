#pragma once

#include "stellar.h"

#include <stdint.h>
#include <stddef.h>

enum session_type
{
	SESSION_TYPE_TCP,
	SESSION_TYPE_UDP,
	__SESSION_TYPE_MAX,
};

enum session_state
{
	SESSION_STATE_INVALID = 0,
	SESSION_STATE_OPENING = 1 ,
	SESSION_STATE_ACTIVE = 2,
	SESSION_STATE_CLOSING = 3,
	SESSION_STATE_CONTROL = 6,
	__SESSION_STATE_MAX,
};

enum session_type session_get_type(struct session *sess);

#define SESSION_SEEN_C2S_FLOW (1 << 0)
#define SESSION_SEEN_S2C_FLOW (1 << 1)
int session_is_symmetric(struct session *sess, unsigned char *flag);

long long session_get_client_isn(struct session *sess);
long long session_get_server_isn(struct session *sess);

#define SESSION_IS_TUNNLE_NON 	 (0) 	/* default is 0, not tunnel; */
#define	SESSION_IS_TUNNLE_6OVER4 	 (1 << 0)
#define	SESSION_IS_TUNNLE_4OVER6	 (1 << 1)
#define	SESSION_IS_TUNNLE_GRE		 (1 << 2)
#define	SESSION_IS_TUNNLE_IP_IN_IP	 (1 << 3)
#define	SESSION_IS_TUNNLE_PPTP		 (1 << 4)
#define	SESSION_IS_TUNNLE_L2TP		 (1 << 5)
#define	SESSION_IS_TUNNLE_TEREDO	 (1 << 6)
#define	SESSION_IS_TUNNLE_GTP (1 << 7)
#define	SESSION_IS_TUNNLE_SOCKS (1 << 8)
#define	SESSION_IS_TUNNLE_HTTP_PROXY (1 << 9)

int session_is_outmost(struct session *sess, uint64_t *flag);
int session_is_innermost(struct session *sess, uint64_t *flag);

#define SESSION_DIRECTION_IN 0
#define SESSION_DIRECTION_OUT 1
int  session_get_direction(struct session *sess);       	

enum session_addr_type
{
	SESSION_ADDR_TYPE_IPV4_TCP,
	SESSION_ADDR_TYPE_IPV4_UDP,
	SESSION_ADDR_TYPE_IPV6_TCP,
	SESSION_ADDR_TYPE_IPV6_UDP,
	SESSION_ADDR_TYPE_UNKNOWN,
	__SESSION_ADDR_TYPE_MAX,
};
struct session_addr_ipv4{
	uint32_t saddr;	/* network order */
	uint32_t daddr;	/* network order */
	uint16_t sport;	/* network order */
	uint16_t dport;	/* network order */
};

#include <netinet/in.h>
#ifndef IPV6_ADDR_LEN
#define IPV6_ADDR_LEN	(sizeof(struct in6_addr))
#endif
struct session_addr_ipv6
{
	uint8_t saddr[IPV6_ADDR_LEN] ;
	uint8_t daddr[IPV6_ADDR_LEN] ;
	uint16_t sport;	/* network order */
	uint16_t dport;	/* network order */
};

struct session_addr
{
	union
	{
		struct session_addr_ipv4 ipv4;
		struct session_addr_ipv6 ipv6;
	};
};

struct session_addr *session_get0_addr(struct session *sess, enum session_addr_type *addr_type);

const char *session_get0_readable_addr(struct session *sess);
const char *session_get0_current_payload(struct session *sess, size_t *payload_len);
enum session_state session_get_current_state(struct session *sess);

int session_get_current_thread_id(struct session *sess);

/* ------------session------------------*/
/*  |l2|l3|l4|session payload|  */
const char *session_get0_current_l3_header(struct session *sess);
const char *session_get0_current_l4_header(struct session *sess);

const char *session_get0_l2_l3_hdr(struct session *sess, int session_direction, size_t *l2_l3_hdr_len);

uint16_t *session_get0_segment_id_list(struct session *sess, int session_direction, size_t *sid_num);
const char *session_get0_route_ctx(struct session *sess, int session_direction, size_t *route_ctx_len);

int session_set_session_id(struct session *sess, uint64_t session_id);
int session_set_preappend_segment_id_list(struct session *sess, uint16_t *sid, size_t sid_num);

const struct packet *session_get0_current_packet(struct session *sess);


//flow direction
#define PACKET_DIRECTION_C2S 0
#define PACKET_DIRECTION_S2C 1
#define PACKET_DIRECTION_UNKNOWN 2
int packet_get_direction(const struct packet *pkt);

//route direction
#define PACKET_DIRECTION_INCOMING 0
#define PACKET_DIRECTION_OUTGOING 1
int packet_get_route_direction(const struct packet *pkt);

const char *packet_get0_data(const struct packet *pkt, size_t *data_len);
int packet_arrive_time(const struct packet *pkt, struct timeval *ts);
unsigned char packet_get_ip_protocol(struct packet *pkt);

void packet_drop(const struct packet *pkt);
const char *packet_get0_readable_addr(struct packet *pkt);