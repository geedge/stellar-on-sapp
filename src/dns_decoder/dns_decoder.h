/*
**********************************************************************************************
*	File: dns_decoder.h
*   Description: dns decoder api
*	Authors: Liu WenTan <liuwentan@geedgenetworks.com>
*	Date:    2024-02-07
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

#ifndef _DNS_DECODER_H_
#define _DNS_DECODER_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <linux/limits.h>
#include <stddef.h>

#define DNS_NAME_MAX (NAME_MAX + 1)
#define HINFO_NAME_MAX (40 + 1)      /* https://www.ietf.org/rfc/rfc1010.txt */

enum dns_message_type {
	DNS_MESSAGE_QUERY,
	DNS_MESSAGE_RESPONSE,
    DNS_MESSAGE_MAX
};

/* RR(resource record) type, defined in https://www.ietf.org/rfc/rfc1035.txt */
enum dns_rr_type {
	DNS_RR_TYPE_A = 1,
	DNS_RR_TYPE_NS,
	DNS_RR_TYPE_MD,           /* Obsolete - use MX, rfc973 */
	DNS_RR_TYPE_MF,           /* Obsolete - use MX, rfc973 */
	DNS_RR_TYPE_CNAME,
	DNS_RR_TYPE_SOA,
	DNS_RR_TYPE_MB,           /* EXPERIMENTAL, rfc883,rfc2505 */
	DNS_RR_TYPE_MG,           /* EXPERIMENTAL, rfc883,rfc2505 */
	DNS_RR_TYPE_MR,           /* EXPERIMENTAL, rfc883 */
	DNS_RR_TYPE_NULL,         /* EXPERIMENTAL, rfc1035 */
	DNS_RR_TYPE_WKS,          /* Not to be relied upon, rfc1123 */
	DNS_RR_TYPE_PTR,
	DNS_RR_TYPE_HINFO,
	DNS_RR_TYPE_MINFO,
	DNS_RR_TYPE_MX,
	DNS_RR_TYPE_TXT,
	DNS_RR_TYPE_RP,
	DNS_RR_TYPE_ISDN = 20,
	DNS_RR_TYPE_AAAA = 28,     /* dns_ipv6 */
	DNS_RR_TYPE_SRV = 33,
	DNS_RR_TYPE_DNAME = 39,
	DNS_RR_TYPE_OPT = 41,
	DNS_RR_TYPE_DS = 43,
	DNS_RR_TYPE_RRSIG = 46,
	DNS_RR_TYPE_NSEC,
	DNS_RR_TYPE_DNSKEY,
	DNS_RR_TYPE_NSEC3 = 50,
	DNS_RR_TYPE_NSEC3PARAM,
	DNS_RR_TYPE_HTTPS = 65,
	DNS_RR_TYPE_AXFR = 252,
	DNS_RR_TYPE_MAILB,
	DNS_RR_TYPE_MAILA,          /* Obsolete - see MX */
	DNS_RR_TYPE_ANY,
	DNS_RR_TYPE_DLV = 32769,    /* DSNSEC Lokkaside Validation */
	DNS_RR_TYPE_UNKNOWN = 65534
};

struct dns_flag {
    uint8_t qr:1;
	uint8_t opcode:4;
	uint8_t aa:1;
	uint8_t tc:1;
	uint8_t rd:1;
	uint8_t ra:1;
	uint8_t z:3;
	uint8_t rcode:4;
};

struct dns_query_question {
	enum dns_rr_type qtype;
	uint16_t qclass;
	uint8_t qname[DNS_NAME_MAX];
};

struct rdata_hinfo {
	uint8_t os_len;
	uint8_t cpu_len;
	uint8_t cpu[HINFO_NAME_MAX];
	uint8_t os[HINFO_NAME_MAX];
};

struct rdata_minfo {
	uint8_t rmailbx[DNS_NAME_MAX];
	uint8_t emailbx[DNS_NAME_MAX];
};

struct rdata_mx {
	uint16_t preference;
	uint8_t exchange[DNS_NAME_MAX];
};

struct rdata_soa {
	uint8_t mname[DNS_NAME_MAX];
	uint8_t rname[DNS_NAME_MAX];
	uint32_t serial;
	uint32_t refresh;
	uint32_t retry;
	uint32_t expire;
	uint32_t minimum;
};

struct rdata_txt {
	uint8_t txt[DNS_NAME_MAX];
	uint8_t size;
};

struct rdata_rp {
	uint8_t mailbox[DNS_NAME_MAX];
	uint8_t txt_rr[DNS_NAME_MAX];
};

struct rdata_null {
	uint8_t null[DNS_NAME_MAX];
	uint8_t size;
};

struct rdata_wks {
	uint8_t protocol;
	uint32_t addr;
	uint32_t size;
	uint8_t *bitmap;
};

struct rdata_srv {
	uint16_t priority;
	uint16_t weight;
	uint16_t port;
	uint8_t target[DNS_NAME_MAX];
};

struct rdata_ds {
	uint16_t key_tag;
	uint8_t algo;
	uint8_t digest_type;
	uint32_t digest_len;
	uint8_t *digest;
};

struct rdata_rrsig {
	uint16_t type_covered;
	uint8_t algo;
	uint8_t labels;
	uint32_t original_ttl;
	uint32_t sig_expiration;
	uint32_t sig_inception;
	uint32_t key_tag;
	uint32_t signature_len;
	uint8_t signer_name[DNS_NAME_MAX];
	uint8_t *signature;
};

struct rdata_nsec {
	uint16_t maps_len;
	uint8_t next_domain[DNS_NAME_MAX];
	uint8_t type_bit_maps[DNS_NAME_MAX];
};

struct rdata_dnskey {
	uint16_t flags;
	uint8_t protocol;
	uint8_t algo;
	uint32_t public_key_len;
	uint8_t *public_key; 
};

struct rdata_nsec3 {
	uint8_t hash_algo;
	uint8_t flags;
	uint8_t salt_len;
	uint8_t hash_len;
	uint16_t iteration;
	uint16_t maps_len;
	uint8_t *salt_value;
	uint8_t *next_hash_owner;
	uint8_t type_bit_maps[DNS_NAME_MAX];
};

struct rdata_nsec3param {
	uint8_t hash_algo;
	uint8_t flags;
	uint8_t salt_len;
	uint16_t iteration;
	uint8_t *salt_value;
};

/* rr is short for resource record */
struct dns_rr {
    uint8_t name[DNS_NAME_MAX];
	enum dns_rr_type type;
	uint16_t rr_class;
	uint32_t ttl;		/* 1byte: extended RCODE; 1byte: version; 2bytes: Z(upper bit) if type is OPT */
	uint16_t rdlength;
	union {
		uint8_t cname[DNS_NAME_MAX];
		uint8_t mb[DNS_NAME_MAX];
		uint8_t md[DNS_NAME_MAX];
		uint8_t mf[DNS_NAME_MAX];
		uint8_t mg[DNS_NAME_MAX];
		uint8_t mr[DNS_NAME_MAX];
		uint8_t ns[DNS_NAME_MAX];
		uint8_t ptr[DNS_NAME_MAX];
		uint8_t a[DNS_NAME_MAX];
		uint8_t aaaa[DNS_NAME_MAX];
		uint8_t dname[DNS_NAME_MAX];
		uint8_t isdn[DNS_NAME_MAX];
		uint8_t unknown_data[DNS_NAME_MAX];
		struct rdata_hinfo hinfo;
		struct rdata_minfo minfo;
		struct rdata_mx mx;
		struct rdata_soa soa;
		struct rdata_txt txt;
		struct rdata_rp rp;
		struct rdata_null null;
		struct rdata_wks wks;
		struct rdata_srv srv;
		struct rdata_ds ds;
		struct rdata_rrsig rrsig;
		struct rdata_nsec nsec;
		struct rdata_dnskey dnskey;
		struct rdata_nsec3 nsec3;
		struct rdata_nsec3param nsec3param;
	} rdata;
};

struct dns_query {
	uint16_t transaction_id;
    struct dns_flag flag;
    uint16_t n_question;
	struct dns_query_question query_question;
};

struct dns_response {
    struct dns_query query;
    uint16_t n_answer_rr;
    uint16_t n_authority_rr;
    uint16_t n_additional_rr;
	struct dns_rr *answer_rr;
    struct dns_rr *authority_rr;
    struct dns_rr *additional_rr;
};

struct dns_message;

enum dns_message_type dns_message_type(struct dns_message *msg);

int dns_message_get_query(struct dns_message *msg, struct dns_query *query);

int dns_message_get_response(struct dns_message *msg, struct dns_response *response);

int dns_serialize_query(uint8_t **buff, size_t *buff_len, struct dns_query *query);

int dns_serialize_response(uint8_t **buff, size_t *buff_len, struct dns_response *response);

#ifdef __cplusplus
}
#endif

#endif
