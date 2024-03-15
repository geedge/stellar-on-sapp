/*
**********************************************************************************************
*	File: dns_decoder.c
*   Description:
*	Authors: Liu WenTan <liuwentan@geedgenetworks.com>
*	Date:    2024-02-07
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "stellar/utils.h"
#include "stellar/session.h"
#include "stellar/stellar.h"
#include "stellar/session_mq.h"
#include "stellar/session_exdata.h"
#include "dns_decoder.h"

#define DNS_HEADER_SIZE 12

#define NS_INT16SZ 2
#define NS_INT32SZ 4

#define NS_GET16(s, cp) do { \
	register uint8_t *t_cp = (uint8_t *)(cp); \
	(s) = ((uint16_t)t_cp[0] << 8) \
	    | ((uint16_t)t_cp[1]); \
	    (cp) += NS_INT16SZ; \
} while (0)

#define NS_GET32(l, cp) do { \
	register uint8_t *t_cp = (uint8_t *)(cp); \
	(l) = ((uint32_t)t_cp[0] << 24) \
	    | ((uint32_t)t_cp[1] << 16) \
	    | ((uint32_t)t_cp[2] << 8) \
	    | ((uint32_t)t_cp[3]); \
	    (cp) += NS_INT32SZ; \
} while (0)

const char *dns_decoder_topic = "DNS_DECODER_MESSAGE";

struct dns_message {
    enum dns_message_type type;
    union {
        struct dns_query query;
        struct dns_response response;
    };
};

struct dns_decoder_context {
    int plugin_id;
    int topic_id;
    int ex_data_idx;
    struct stellar *st;
};

struct dns_decoder_exdata {
    char *cache_ptr;
    int one_byte_len;
    size_t cache_len;
    size_t cache_capacity;
};

struct dns_header {
    uint16_t id;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint8_t rd:1;
	uint8_t tc:1;
	uint8_t aa:1;
	uint8_t opcode:4;
	uint8_t qr:1;
	uint8_t rcode:4;
	uint8_t z:3;
	uint8_t ra:1;
#elif __BYTE_ORDER == __BIG_ENDIAN
	uint8_t qr:1;
	uint8_t opcode:4;
	uint8_t aa:1;
	uint8_t tc:1;
	uint8_t rd:1;
	uint8_t ra:1;
	uint8_t z:3;
	uint8_t rcode:4;
#endif
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t aucount; //authority count
	uint16_t adcount; //additional count
};

struct dns_message *
dns_message_new(enum dns_message_type type)
{
    struct dns_message *msg = CALLOC(struct dns_message, 1);

    msg->type = type;

    return msg;
}

static void dns_message_free(void *dns_msg, void *cb_arg)
{
    if (NULL == dns_msg) {
		return;
	}

    struct dns_message *msg = (struct dns_message *)dns_msg;
    if (msg->type == DNS_MESSAGE_RESPONSE) {
        if (msg->response.answer_rr != NULL) {
            FREE(msg->response.answer_rr);
        }

        if (msg->response.authority_rr != NULL) {
            FREE(msg->response.authority_rr);
        }

        if (msg->response.additional_rr != NULL) {
            FREE(msg->response.additional_rr);
        }
    }

    FREE(msg);
}

enum dns_message_type dns_message_type(struct dns_message *msg) {
    if (NULL == msg) {
        return DNS_MESSAGE_MAX;
    }

    return msg->type;
}

int dns_message_get_query(struct dns_message *msg, struct dns_query *query)
{
    if (NULL == msg || msg->type != DNS_MESSAGE_QUERY || NULL == query) {
        return -1;
    }

    *query = msg->query;
    return 0;
}

int dns_message_get_response(struct dns_message *msg,
                             struct dns_response *response)
{
    if (NULL == msg || msg->type != DNS_MESSAGE_RESPONSE ||
        NULL == response) {
        return -1;
    }
    *response = msg->response;
    return 0;
}

int dns_serialize_query(uint8_t **buff, size_t *buff_len, struct dns_query *query)
{
    return 0;
}

int dns_serialize_response(uint8_t **buff, size_t *buff_len, struct dns_response *response)
{
    return 0;
}

static void
dns_decoder_ex_data_free(struct session *s, int idx, void *ex_data, void *arg)
{

}

static void _dns_decoder_context_free(struct dns_decoder_context *ctx)
{
    if (NULL == ctx) {
        return;
    }

    if (ctx->topic_id >= 0) {
        session_mq_destroy_topic(ctx->st, ctx->topic_id);
        ctx->topic_id = -1;
    }

    FREE(ctx);
}

static void populate_dns_header(struct dns_header *dns_hdr, const uint8_t *payload)
{
	struct dns_header *tmp = (struct dns_header *)payload;

	dns_hdr->qr = tmp->qr;
	dns_hdr->opcode= tmp->opcode;
	dns_hdr->aa = tmp->aa;
	dns_hdr->tc = tmp->tc;
	dns_hdr->rd = tmp->rd;
	dns_hdr->ra = tmp->ra;
	dns_hdr->z = tmp->z;
	dns_hdr->rcode = tmp->rcode;

	dns_hdr->id = ntohs(tmp->id);
	dns_hdr->qdcount = ntohs(tmp->qdcount);
	dns_hdr->ancount = ntohs(tmp->ancount);
	dns_hdr->aucount = ntohs(tmp->aucount);
	dns_hdr->adcount = ntohs(tmp->adcount);
}

static int validate_dns_header(struct dns_header *dns_hdr)
{ 

    return 0;
}

static int
get_decompressed_name(uint8_t *dns_payload, size_t dns_payload_len,
                      uint8_t **cur_pos, uint8_t *buf, size_t buf_len)
{
	size_t index = 0;
	size_t len = 0;
	size_t tot_len = 0;
	uint8_t np = 0;
	uint8_t *cursor = *cur_pos;
	uint8_t *dns_payload_end = dns_payload + dns_payload_len - 1;
	*cur_pos = NULL;

	while (cursor <= dns_payload_end) {
		if (0 == cursor[0]) {
			break;
		}

		if (0x0c0 == (cursor[0] & 0x0c0)) {
			if(cursor + 1 > dns_payload_end) {
				return -1;
			}
			
			/* udp payload offset */
			len = ((cursor[0] & 0x03f) << 8) + cursor[1];

			if (NULL == *cur_pos) {
				tot_len += 2;
				*cur_pos = cursor + 2;
			}

			/* udp payload offset = dns_header (12bytes) + dns_payload */
			cursor = dns_payload + len - 12;
			if (cursor > dns_payload_end) {
				return -1;
			}
				
			/* too many pointers. */
			if (np++ > 16) {
				return -1;
			}
				
			continue;
		}

		len = cursor[0];
		cursor++;
		tot_len++;

		if (cursor + len - 1 > dns_payload_end) {
			return -1;
		}
			
		if (index + len >= (buf_len - 1)) {
			return -1;
		}
			
		memcpy(buf + index, cursor, len);
		index += len;
		buf[index++] = '.';
		cursor += len;
		tot_len += len;
	}
	
	if (NULL == *cur_pos) {
		*cur_pos = cursor + 1;
		tot_len++;
	}
	
	/* omit last '.' */
	if (index > 0) {
		buf[index - 1] = '\0';
	} else {
		buf[0] = '\0';
	}
	
	return tot_len;
}

static int get_rr_domain(uint8_t *dns_payload, size_t dns_payload_len,
                         uint8_t **cur_pos, uint8_t *buf, size_t buf_len)
{
	return get_decompressed_name(dns_payload, dns_payload_len,
                                 cur_pos, buf, buf_len);
}

static int
parse_dns_query_question(struct dns_query_question *question,
                         uint8_t *dns_payload, size_t dns_payload_len,
                         uint8_t **cur_pos)
{
    *cur_pos = dns_payload;
	uint8_t *dns_payload_end = dns_payload + dns_payload_len - 1;

	if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                      question->qname, DNS_NAME_MAX) <= 0) {
		return -1;
	}
	
	if (*cur_pos + 1 > dns_payload_end) {
		return -1;
	}
	NS_GET16(question->qtype, *cur_pos);

	if (*cur_pos + 1 > dns_payload_end) {
		return -1;
	}
	NS_GET16(question->qclass, *cur_pos);
	
	return 0;
}

static void dns_flag_copy(struct dns_flag *flag, struct dns_header *dns_hdr)
{
    flag->qr = dns_hdr->qr;
    flag->opcode = dns_hdr->opcode;
    flag->aa = dns_hdr->aa;
    flag->tc = dns_hdr->tc;
    flag->rd = dns_hdr->rd;
    flag->ra = dns_hdr->ra;
    flag->z = dns_hdr->z;
    flag->rcode = dns_hdr->rcode;
}

static int
parse_dns_query(struct dns_query *query, struct dns_header *dns_hdr,
                uint8_t *dns_payload, size_t dns_payload_len)
{
    query->transaction_id = dns_hdr->id;

    //dns query flag
    dns_flag_copy(&query->flag, dns_hdr);

    query->n_question = dns_hdr->qdcount;

    uint8_t *cur_pos = NULL;
    int ret = parse_dns_query_question(&query->query_question, dns_payload,
                                       dns_payload_len, &cur_pos);
    if (ret < 0) {
        fprintf(stderr, "parse dns query question failed.\n");
        return -1;
    }

    return 0;
}

static void
dns_response_init(struct dns_response *response, struct dns_header *dns_hdr)
{
    struct dns_query *query = &response->query;

    query->transaction_id = dns_hdr->id;
    dns_flag_copy(&query->flag, dns_hdr);

    query->n_question = dns_hdr->qdcount;

    response->n_answer_rr = dns_hdr->ancount;
    response->n_authority_rr = dns_hdr->aucount;
    response->n_additional_rr = dns_hdr->adcount;

    if (response->n_answer_rr > 0) {
        response->answer_rr = CALLOC(struct dns_rr, response->n_answer_rr);
    }

    if (response->n_authority_rr > 0) {
        response->authority_rr = CALLOC(struct dns_rr, response->n_authority_rr);
    }

    if (response->n_additional_rr > 0) {
        response->additional_rr = CALLOC(struct dns_rr, response->n_additional_rr);
    }
}

static int
get_rr_common_field(uint8_t *dns_payload, size_t dns_payload_len,
                    uint8_t **cur_pos, struct dns_rr *dns_rr)
{
	uint8_t *dns_payload_end = dns_payload + dns_payload_len - 1;

	if (NULL == *cur_pos)  {
		return -1;
	}
	
	if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                      dns_rr->name, DNS_NAME_MAX) <= 0) {
		return -1;
	}

	if (NULL == *cur_pos || (*cur_pos + 1) > dns_payload_end) {
		return -1;
	}
	NS_GET16(dns_rr->type, *cur_pos);
	
	if (dns_rr->type == DNS_RR_TYPE_OPT) {
		return 0;
	}
	
	if (NULL == *cur_pos || (*cur_pos + 1) > dns_payload_end) {
		return -1;
	}
	NS_GET16(dns_rr->rr_class, *cur_pos);

	if (NULL == *cur_pos || (*cur_pos + 3) > dns_payload_end) {
		return -1;
	}
	NS_GET32(dns_rr->ttl, *cur_pos);

	if (NULL == *cur_pos || (*cur_pos + 1) > dns_payload_end) {
		return -1;
	}
	NS_GET16(dns_rr->rdlength, *cur_pos);

	if (NULL == *cur_pos ||
        (*cur_pos + dns_rr->rdlength - 1) > dns_payload_end) {
		return -1;
	}

	return 0;
}

static int get_rr_type_info(uint8_t **cur_pos, struct rdata_hinfo *hinfo)
{	
	hinfo->cpu_len = (*cur_pos)[0];
	*cur_pos += 1;

	uint8_t length = MIN(hinfo->cpu_len, sizeof(hinfo->cpu) - 1);
	if (length > 0) {
		memcpy(hinfo->cpu, *cur_pos, length);
		hinfo->cpu[length] = '\0';
	} else {
		hinfo->cpu[0] = '\0';
	}
	
	*cur_pos += hinfo->cpu_len;
	hinfo->cpu_len = (length > 0 ? length : 0);
	
	hinfo->os_len = (*cur_pos)[0];
	*cur_pos += 1;

	length = MIN(hinfo->os_len, sizeof(hinfo->os) - 1);
	if (length > 0) {
		memcpy(hinfo->os, *cur_pos, length);
		hinfo->os[length] = '\0';
	} else {
		hinfo->os[0] = '\0';
	}
	
	*cur_pos += hinfo->os_len;
	hinfo->os_len = (length > 0 ? length : 0);
	
	return 0;
}

static int
get_rr_type_soa(uint8_t *dns_payload, size_t dns_payload_len,
                uint8_t **cur_pos, struct rdata_soa *soa)
{
	uint8_t *dns_payload_end = dns_payload + dns_payload_len - 1;

	if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                      soa->mname, sizeof(soa->mname)) <= 0) {
		return -1;
	}

	if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                      soa->rname, sizeof(soa->rname)) <= 0) {
		return -1;
	}
	
	if ((*cur_pos + 3) > dns_payload_end) {
		return -1;
	}
	NS_GET32(soa->serial, *cur_pos);

	if ((*cur_pos + 3) > dns_payload_end) {
		return -1;
	}
	NS_GET32(soa->refresh, *cur_pos);

	if ((*cur_pos + 3) > dns_payload_end) {
		return -1;
	}
	NS_GET32(soa->retry, *cur_pos);

	if ((*cur_pos + 3) > dns_payload_end) {
		return -1;
	}
	NS_GET32(soa->expire, *cur_pos);

	if ((*cur_pos + 3) > dns_payload_end) {
		return -1;
	}
	NS_GET32(soa->minimum, *cur_pos);

	return 0;
}

static int
get_rr_type_wks(uint8_t **cur_pos, struct rdata_wks *wks,
                const uint8_t *dns_payload_end)
{
	if ((*cur_pos + 3) > dns_payload_end) {
		return -1;
	}
	NS_GET32(wks->addr, *cur_pos);

	if ((*cur_pos + 1) > dns_payload_end) {
		return -1;
	}

	wks->protocol = **cur_pos;
	*cur_pos += 1;
	wks->bitmap = *cur_pos;

	return 0;
}

static int
get_rr_type_rrsig(uint8_t **cur_pos, struct rdata_rrsig *rrsig,
                  const uint8_t *dns_payload_end)
{
	if ((*cur_pos + 1) > dns_payload_end) {
		return -1;
	}
	NS_GET16(rrsig->type_covered, *cur_pos);

	rrsig->algo = **cur_pos;
	*cur_pos += 1;
	rrsig->labels = **cur_pos;
	*cur_pos += 1;

	if ((*cur_pos + 13) > dns_payload_end) {
		return -1;
	}

	NS_GET32(rrsig->original_ttl, *cur_pos);
	NS_GET32(rrsig->sig_expiration, *cur_pos);
	NS_GET32(rrsig->sig_inception, *cur_pos);
	NS_GET16(rrsig->key_tag, *cur_pos);

	return 0;
}

static int
dissect_type_bitmap(uint8_t *cur_pos, size_t rr_len, uint8_t *maps_buff,
                    uint16_t *maps_len)
{
	size_t cur_offset = 0;
	size_t map_offset = 0;

	if (NULL == cur_pos || 0 == rr_len) {
		*maps_len = 0;
		return 0;
	}

	while ((rr_len - cur_offset) > 0) {
		if (map_offset + 2 > *maps_len) {
			break;
		}

		maps_buff[map_offset++] = cur_pos[cur_offset++];		
		maps_buff[map_offset++] = cur_pos[cur_offset];
		size_t blocksize = cur_pos[cur_offset++];	

		size_t length = MIN(*maps_len - map_offset, blocksize);
		if (length == 0 || blocksize > (rr_len - cur_offset)) {
			break;
		}
		
		memcpy(maps_buff + map_offset, cur_pos + cur_offset, length);
		cur_offset += blocksize;
		map_offset += length;
	}

	*maps_len = map_offset;

	return cur_offset;
}

static int
get_rr_type_nsec3(uint8_t **cur_pos, struct rdata_nsec3 *nsec3,
                  const uint8_t *dns_payload_end)
{
	if ((*cur_pos + 4) > dns_payload_end) {
		return -1;
	}

	nsec3->hash_algo = **cur_pos;
	*cur_pos += 1;
	nsec3->flags= **cur_pos;
	*cur_pos += 1;

	NS_GET16(nsec3->iteration, *cur_pos);

	nsec3->salt_len = **cur_pos;
	*cur_pos += 1;

	if (nsec3->salt_len > 0) {
		if ((*cur_pos + nsec3->salt_len) > dns_payload_end) {
			return -1;
		}

		nsec3->salt_value = *cur_pos;
		*cur_pos += nsec3->salt_len;	/* jump salt_value */
	}
	
	nsec3->hash_len = **cur_pos;
	*cur_pos += 1;

	if (nsec3->hash_len > 0) {
		if ((*cur_pos + nsec3->hash_len) > dns_payload_end) {
			return -1;
		}

		nsec3->next_hash_owner = *cur_pos;	
		*cur_pos += nsec3->hash_len; /* jump next_hash_owner */
	}
	
	return 0;
}

static int
get_one_rr(uint8_t *dns_payload, size_t dns_payload_len, uint8_t **cur_pos,
           struct dns_rr *dns_rr)
{
	int len = 0;
	uint8_t *original_ptr = NULL;
	uint8_t *dns_payload_end =dns_payload + dns_payload_len - 1;

	switch(dns_rr->type) {
		case DNS_RR_TYPE_CNAME:
			if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                              dns_rr->rdata.cname, DNS_NAME_MAX) <= 0) {
				dns_rr->rdata.cname[0] = '\0';
				return -1;
			}
			
			break;
		case DNS_RR_TYPE_HINFO:
			if (get_rr_type_info(cur_pos, &(dns_rr->rdata.hinfo)) != 0) {
				memset(&(dns_rr->rdata.hinfo), 0, sizeof(struct rdata_hinfo));
				return -1;
			}
			break;
		case DNS_RR_TYPE_MB:
			if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                              dns_rr->rdata.mb, DNS_NAME_MAX) <= 0) {
				dns_rr->rdata.mb[0] = '\0';
				return -1;
			}

			break;
		case DNS_RR_TYPE_MD:
			if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                              dns_rr->rdata.md, DNS_NAME_MAX) <= 0) {
				dns_rr->rdata.md[0] = '\0';
				return -1;
			}

			break;
		case DNS_RR_TYPE_MF:
			if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                              dns_rr->rdata.mf, DNS_NAME_MAX) <= 0) {
				dns_rr->rdata.mf[0] = '\0';
				return -1;
			}

			break;
		case DNS_RR_TYPE_MG:
			if(get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                             dns_rr->rdata.mg, DNS_NAME_MAX) <= 0) {
				dns_rr->rdata.mg[0] = '\0';
				return -1;
			}

			break;
		case DNS_RR_TYPE_MINFO:
			if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                              dns_rr->rdata.minfo.rmailbx, DNS_NAME_MAX) <= 0) {
				dns_rr->rdata.minfo.rmailbx[0] = '\0';
				return -1;
			}
			
			if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                              dns_rr->rdata.minfo.emailbx, DNS_NAME_MAX) <= 0) {
				dns_rr->rdata.minfo.emailbx[0] = '\0';
				return -1;
			}
			break;
		case DNS_RR_TYPE_MR:
			if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                              dns_rr->rdata.mr, DNS_NAME_MAX) <= 0) {
				dns_rr->rdata.mr[0] = '\0';
				return -1;
			}
			break;
		case DNS_RR_TYPE_MX:
			if((*cur_pos + 1) > dns_payload_end) {
				return -1;
			} 
			
			NS_GET16(dns_rr->rdata.mx.preference, *cur_pos);

			if ((dns_rr->rdlength - 2) < (*cur_pos)[0]) {
				if (dns_rr->rdlength < 2) {
					*cur_pos += dns_rr->rdlength;
					break;
				}

				len = MIN(DNS_NAME_MAX - 2, dns_rr->rdlength - 2);/*size=1byte*/
				memcpy(dns_rr->rdata.mx.exchange, *cur_pos, len);	/* error labels */
				dns_rr->rdata.mx.exchange[len] = '\0';
				*cur_pos += dns_rr->rdlength - 2;
			} else {
				if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                                  dns_rr->rdata.mx.exchange, DNS_NAME_MAX) <= 0) {
					dns_rr->rdata.mx.exchange[0] = '\0';
					return -1;
				}
			}
			break;
		case DNS_RR_TYPE_NS:
			if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                              dns_rr->rdata.ns, DNS_NAME_MAX) <= 0) {
				dns_rr->rdata.ns[0] = '\0';
				return -1;
			}
			break;
		case DNS_RR_TYPE_PTR:
			if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                              dns_rr->rdata.ptr, DNS_NAME_MAX) <= 0) {
				dns_rr->rdata.ptr[0] = '\0';
				return -1;
			}
			break;
		case DNS_RR_TYPE_SOA:
			original_ptr = *cur_pos;
			if (get_rr_type_soa(dns_payload, dns_payload_len, cur_pos,
                                &(dns_rr->rdata.soa)) != 0) {
				memset(&(dns_rr->rdata.soa), 0, sizeof(struct rdata_soa));
				return -1;
			}

			if(original_ptr + dns_rr->rdlength != *cur_pos) {
				*cur_pos = original_ptr + dns_rr->rdlength;
			}
			break;
		case DNS_RR_TYPE_A:
			if ((*cur_pos + 3) > dns_payload_end) {
				return -1;
			}

			memcpy(dns_rr->rdata.a, *cur_pos, NS_INT32SZ);
			(*cur_pos) += NS_INT32SZ;
			break;
		case DNS_RR_TYPE_AAAA:
			if((*cur_pos + 15) > dns_payload_end) {
				return -1;
			}

			memcpy(dns_rr->rdata.aaaa, *cur_pos, 16);
			(*cur_pos) += 16;
			break;
		case DNS_RR_TYPE_DNAME:
			if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                              dns_rr->rdata.dname, DNS_NAME_MAX) <= 0) {
				dns_rr->rdata.dname[0] = '\0';
				return -1;
			}
			break;
		case DNS_RR_TYPE_ISDN:
			memcpy(dns_rr->rdata.isdn, *cur_pos, sizeof(uint8_t));
			(*cur_pos) += 1;
			break;
		case DNS_RR_TYPE_TXT:
			len = MIN(DNS_NAME_MAX - 2, dns_rr->rdlength - 1); /*size=1byte*/
			memcpy(dns_rr->rdata.txt.txt, *cur_pos + 1, len);
			dns_rr->rdata.txt.size = len;
			*cur_pos += dns_rr->rdlength;
			break;
		case DNS_RR_TYPE_RP:
			if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                              dns_rr->rdata.rp.mailbox, DNS_NAME_MAX) <= 0) {
				dns_rr->rdata.rp.mailbox[0] = '\0';
				return -1;
			}

			if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                              dns_rr->rdata.rp.txt_rr, DNS_NAME_MAX) <= 0) {
				dns_rr->rdata.rp.txt_rr[0] = '\0';
				return -1;
			}
			break;
		case DNS_RR_TYPE_NULL:
			len = MIN(DNS_NAME_MAX-2, dns_rr->rdlength - 1);	/*size=1byte*/
			memcpy(dns_rr->rdata.null.null, *cur_pos + 1, len);
			dns_rr->rdata.null.size = len;
			*cur_pos += dns_rr->rdlength;
			break;
		case DNS_RR_TYPE_WKS:
			if (get_rr_type_wks(cur_pos, &(dns_rr->rdata.wks), dns_payload_end) != 0) {
				memset(&(dns_rr->rdata.wks), 0, sizeof(struct rdata_wks));
				return -1;
			}

			dns_rr->rdata.wks.size = dns_rr->rdlength - 5;
			if (0 == dns_rr->rdata.wks.size) {
				dns_rr->rdata.wks.size = 0;
				dns_rr->rdata.wks.bitmap = NULL;
				
			}
			*cur_pos += dns_rr->rdlength - 5;
		case DNS_RR_TYPE_SRV:
			NS_GET16(dns_rr->rdata.srv.priority, *cur_pos);
			NS_GET16(dns_rr->rdata.srv.weight, *cur_pos);
			NS_GET16(dns_rr->rdata.srv.port, *cur_pos);
			if (get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                              dns_rr->rdata.srv.target, DNS_NAME_MAX) <= 0) {
				dns_rr->rdata.srv.target[0] = '\0';
				return -1;
			}
			break;
		case DNS_RR_TYPE_OPT:
			break;
		case DNS_RR_TYPE_DS:
		case DNS_RR_TYPE_DLV:
			if ((*cur_pos + 3) > dns_payload_end) {
				return -1;
			}

			NS_GET16(dns_rr->rdata.ds.key_tag, *cur_pos);
			dns_rr->rdata.ds.algo = **cur_pos;
			*cur_pos += 1;
			dns_rr->rdata.ds.digest_type = **cur_pos;
			*cur_pos += 1;
			dns_rr->rdata.ds.digest = *cur_pos;
			dns_rr->rdata.ds.digest_len = dns_rr->rdlength - 4;
			if (dns_rr->rdata.ds.digest_len == 0) {
				dns_rr->rdata.ds.digest = NULL;
				dns_rr->rdata.ds.digest_len = 0;
			}
			*cur_pos += dns_rr->rdlength - 4;
			break;
		case DNS_RR_TYPE_RRSIG:
			if((*cur_pos + 17) > dns_payload_end) {
				return -1;
			}

			memset(&(dns_rr->rdata.rrsig), 0, sizeof(struct rdata_rrsig));
			get_rr_type_rrsig(cur_pos, &(dns_rr->rdata.rrsig), dns_payload_end);
			len = get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                                dns_rr->rdata.rrsig.signer_name, DNS_NAME_MAX);
			if (len <= 0) {
				dns_rr->rdata.rrsig.signer_name[0] = '\0';
				return -1;
			}

			dns_rr->rdata.rrsig.signature = *cur_pos;
			dns_rr->rdata.rrsig.signature_len = dns_rr->rdlength - 18 - len;
			if (dns_rr->rdata.rrsig.signature_len == 0) {
				dns_rr->rdata.rrsig.signature = NULL;
				dns_rr->rdata.rrsig.signature_len = 0;
			}
			*cur_pos += dns_rr->rdlength - 18 - len;
			break;
		case DNS_RR_TYPE_NSEC:
			original_ptr = *cur_pos;
			len = get_rr_domain(dns_payload, dns_payload_len, cur_pos,
                                dns_rr->rdata.nsec.next_domain, DNS_NAME_MAX);
			if (len <= 0) {
				dns_rr->rdata.nsec.next_domain[0] = '\0';
				return -1;
			}

			if ((int)(dns_rr->rdlength - len) < (int)(sizeof(dns_rr->rdata.nsec.type_bit_maps))) {
				dns_rr->rdata.nsec.maps_len = DNS_NAME_MAX;
				dissect_type_bitmap(*cur_pos, dns_rr->rdlength - len,
                                    (dns_rr->rdata.nsec.type_bit_maps),
                                    &(dns_rr->rdata.nsec.maps_len));
			}
			*cur_pos = original_ptr + dns_rr->rdlength;			
			break;
		case DNS_RR_TYPE_DNSKEY:
			if((*cur_pos + 3) > dns_payload_end) {
				return -1;
			}

			NS_GET16(dns_rr->rdata.dnskey.flags, *cur_pos);
			dns_rr->rdata.dnskey.protocol = **cur_pos;
			*cur_pos += 1;
			dns_rr->rdata.dnskey.algo = **cur_pos;
			*cur_pos += 1;
			dns_rr->rdata.dnskey.public_key = *cur_pos;
			dns_rr->rdata.dnskey.public_key_len = dns_rr->rdlength - 4; /* sizeof(flags)+sizeof(protocol)+sizeof(algo) */
			if (dns_rr->rdata.dnskey.public_key_len == 0) {
				dns_rr->rdata.dnskey.public_key = NULL;
				dns_rr->rdata.dnskey.public_key_len = 0;
			}
			*cur_pos +=  dns_rr->rdlength - 4; /* todo add log */
			break;
		case DNS_RR_TYPE_NSEC3:
			original_ptr = *cur_pos;
			if (get_rr_type_nsec3(cur_pos, &(dns_rr->rdata.nsec3), dns_payload_end) < 0) {
				return -1;
			}

			len = *cur_pos - original_ptr;
			if ((int)(dns_rr->rdlength - len) < (int)(sizeof(dns_rr->rdata.nsec3.type_bit_maps))) {
				dns_rr->rdata.nsec3.maps_len = DNS_NAME_MAX;
				dissect_type_bitmap(*cur_pos, dns_rr->rdlength - len,
                                    (dns_rr->rdata.nsec3.type_bit_maps),
                                    &(dns_rr->rdata.nsec3.maps_len));
			}
			*cur_pos = original_ptr + dns_rr->rdlength;			
			break;
		case DNS_RR_TYPE_NSEC3PARAM:
			dns_rr->rdata.nsec3param.hash_algo = **cur_pos;
			*cur_pos += 1;

			dns_rr->rdata.nsec3param.flags = **cur_pos;
			*cur_pos += 1;

			NS_GET16(dns_rr->rdata.nsec3param.iteration, *cur_pos);

			dns_rr->rdata.nsec3param.salt_len = dns_rr->rdlength - 4 - 1;
			*cur_pos += 1;

			if (dns_rr->rdata.nsec3param.salt_len == 0) {
				dns_rr->rdata.nsec3param.salt_value = NULL;
				dns_rr->rdata.nsec3param.salt_len = 0;
			} else {
				dns_rr->rdata.nsec3param.salt_value = *cur_pos;
			}
			*cur_pos +=  dns_rr->rdlength - 5;
			break;
		case DNS_RR_TYPE_UNKNOWN:
			len = MIN(dns_rr->rdlength, sizeof(dns_rr->rdata.unknown_data) - 1);
			memcpy(dns_rr->rdata.unknown_data, *cur_pos, len);
			dns_rr->rdata.unknown_data[len] = '\0';
			(*cur_pos) += dns_rr->rdlength;
			break;
		default:
			*cur_pos += dns_rr->rdlength;
			fprintf(stderr, "No support dns rr type, type: %d", dns_rr->type);
			break;
	}
	
	return 0;
}

static int
parse_common_dns_rr(uint8_t *dns_payload, size_t dns_payload_len,
					uint8_t **cur_pos, struct dns_rr *rr, size_t n_rr)
{
	uint8_t *dns_payload_end = dns_payload + dns_payload_len - 1;

    for (size_t i = 0; i < n_rr; i++) {
		if (*cur_pos < dns_payload || *cur_pos >= dns_payload_end) {
			return -1;
		}

		if (get_rr_common_field(dns_payload, dns_payload_len,
								cur_pos, &rr[i]) < 0) {
			return -1;
		}

		if (get_one_rr(dns_payload, dns_payload_len, cur_pos, &rr[i]) < 0) {
			return -1;
		}
	}

	return 0;
}

static int parse_dns_rr(struct dns_response *response, uint8_t *dns_payload,
                        size_t dns_payload_len, uint8_t *cur_pos)
{
	size_t i = 0;
	uint8_t *dns_payload_end = dns_payload + dns_payload_len - 1;

	/* parse answer */
	int ret = parse_common_dns_rr(dns_payload, dns_payload_len, &cur_pos,
								  response->answer_rr, response->n_answer_rr);
	if (ret < 0) {
        return -1;
    }

	ret = parse_common_dns_rr(dns_payload, dns_payload_len, &cur_pos,
							  response->authority_rr, response->n_authority_rr);
	if (ret < 0) {
        return -1;
    }

	ret = parse_common_dns_rr(dns_payload, dns_payload_len, &cur_pos,
							  response->additional_rr, response->n_additional_rr);
	if (ret < 0) {
        return -1;
    }

	return 0;
}

static int
parse_dns_response(struct dns_response *response, struct dns_header *dns_hdr,
                   uint8_t *dns_payload, size_t dns_payload_len)
{
    dns_response_init(response, dns_hdr);

    uint8_t *cur_pos = NULL;
    int ret = parse_dns_query_question(&response->query.query_question,
                                       dns_payload, dns_payload_len, &cur_pos);
    if (ret < 0) {
        fprintf(stderr, "parse dns response question failed.\n");
        return -1;
    }

    ret = parse_dns_rr(response, dns_payload, dns_payload_len, cur_pos);
    if (ret < 0) {
        fprintf(stderr, "parse dns reponse resource record failed.\n");
        return -1;
    }

    return 0;
}

static int
udp_dns_decoder_entry(struct session *sess, int events, const struct packet *pkt,
                      struct dns_decoder_context *ctx)
{
    size_t udp_payload_len = 0;

    uint8_t *udp_payload =
		(uint8_t *)session_get0_current_payload(sess, &udp_payload_len);
    if (udp_payload_len < DNS_HEADER_SIZE) {
        //Non dns packet
        return 0;
    }

    struct dns_header dns_hdr = {0};
    populate_dns_header(&dns_hdr, udp_payload);

    int ret = validate_dns_header(&dns_hdr);
    if (ret < 0) {
        return 0;
    }

    uint8_t *dns_payload = udp_payload + DNS_HEADER_SIZE;
    size_t dns_payload_len = udp_payload_len - DNS_HEADER_SIZE;

    struct dns_message *msg = NULL;
    if (0 == dns_hdr.qr) {
        //query
        msg = dns_message_new(DNS_MESSAGE_QUERY);
        ret = parse_dns_query(&msg->query, &dns_hdr, dns_payload,
                              dns_payload_len);
        if (ret < 0) {
            dns_message_free(msg, NULL);
            return -1;
        }
    } else {
        //response
        msg = dns_message_new(DNS_MESSAGE_RESPONSE);
        ret = parse_dns_response(&msg->response, &dns_hdr, dns_payload,
                                 dns_payload_len);
        if (ret < 0) {
            dns_message_free(msg, NULL);
            return -1;
        }
    }

    session_mq_publish_message(sess, ctx->topic_id, msg);

    return 0;
}

static void
dns_decoder_exdata_free(struct dns_decoder_exdata *ex_data)
{
	if (NULL == ex_data) {
		return;
	}

	if (ex_data->cache_ptr != NULL) {
		FREE(ex_data->cache_ptr);
	}

	FREE(ex_data);
}

static int
tcp_dns_decoder_entry(struct session *sess, int events, const struct packet *pkt,
                      struct dns_decoder_context *ctx)
{
    struct dns_decoder_exdata *ex_data =
		session_get_ex_data(sess, ctx->ex_data_idx);

	if (events & SESS_EV_CLOSING) {
		if (ex_data != NULL) {
			dns_decoder_exdata_free(ex_data);
			session_set_ex_data(sess, ctx->ex_data_idx, NULL);
		}

		return 0;
	}

	size_t tcp_payload_len = 0;
	uint8_t *tcp_payload =
		(uint8_t *)session_get0_current_payload(sess, &tcp_payload_len);
	if (0 == tcp_payload_len) {
		return 0;
	}

	uint8_t *buff = NULL;
	size_t buff_len = 0;

	if (NULL == ex_data) {
		if (1 == tcp_payload_len) {
			//cache this byte
			ex_data = CALLOC(struct dns_decoder_exdata, 1);
			ex_data->one_byte_len = *(uint8_t *)tcp_payload;
			session_set_ex_data(sess, ctx->ex_data_idx, ex_data);
			return 0;
		} else {
			uint16_t dns_len = ntohs(*(uint16_t *)tcp_payload);
			if (dns_len > (tcp_payload_len - 2)) {
				ex_data = CALLOC(struct dns_decoder_exdata, 1);
				ex_data->one_byte_len = -1;
				ex_data->cache_ptr = CALLOC(char, dns_len);	
				ex_data->cache_len = tcp_payload_len - 2;
				ex_data->cache_capacity = dns_len;
				memcpy(ex_data->cache_ptr, tcp_payload, ex_data->cache_len);
				session_set_ex_data(sess, ctx->ex_data_idx, ex_data);

				return 0;
			} else {
				//goto dns parser
                buff = tcp_payload + 2;
                buff_len = tcp_payload_len - 2;
                goto next;
            }
		}
	} else {
        // SESS_EV_PACKET
        if (ex_data->one_byte_len == -1) {
            if (tcp_payload_len < ex_data->cache_capacity - ex_data->cache_len) {
                memcpy(ex_data->cache_ptr + ex_data->cache_len, tcp_payload,
                       tcp_payload_len);
                ex_data->cache_len += tcp_payload_len;
                return 0;
            } else if (tcp_payload_len == ex_data->cache_capacity - ex_data->cache_len) {
                memcpy(ex_data->cache_ptr + ex_data->cache_len, tcp_payload,
                       tcp_payload_len);
                ex_data->cache_len += tcp_payload_len;
                buff = ex_data->cache_ptr;
                buff_len = ex_data->cache_len;
            } else {
                assert(0);
            }
        } else {
			//
            uint8_t tmp_len[2] = {0};
            tmp_len[0] = ex_data->one_byte_len;
            tmp_len[1] = *(uint8_t *)tcp_payload;
            uint16_t dns_len = ntohs(*(uint16_t *)tmp_len);
			if (dns_len > (tcp_payload_len - 1)) {
				memcpy(ex_data->cache_ptr + ex_data->cache_len, tcp_payload + 1,
					   tcp_payload_len - 1);
				ex_data->cache_len += tcp_payload_len - 1;
				return 0;
			} else {
				buff = tcp_payload + 1;
				buff_len = tcp_payload_len - 1;
			}
        }
    }

    struct dns_header dns_hdr = {0};
next:
    populate_dns_header(&dns_hdr, buff);

    int ret = validate_dns_header(&dns_hdr);
    if (ret < 0) {
        return 0;
    }

    uint8_t *dns_payload = buff + DNS_HEADER_SIZE;
    size_t dns_payload_len = buff_len - DNS_HEADER_SIZE;

    struct dns_message *msg = NULL;
    if (0 == dns_hdr.qr) {
        //query
        msg = dns_message_new(DNS_MESSAGE_QUERY);
        ret = parse_dns_query(&msg->query, &dns_hdr, dns_payload,
                              dns_payload_len);
        if (ret < 0) {
            dns_message_free(msg, NULL);
            return -1;
        }
    } else {
        //response
        msg = dns_message_new(DNS_MESSAGE_RESPONSE);
        ret = parse_dns_response(&msg->response, &dns_hdr, dns_payload,
                                 dns_payload_len);
        if (ret < 0) {
            dns_message_free(msg, NULL);
            return -1;
        }
    }

    session_mq_publish_message(sess, ctx->topic_id, msg);

    return 0;
}

int dns_decoder_entry(struct session *sess, int events,
                      const struct packet *pkt, void *cb_arg)
{
    struct dns_decoder_context *ctx = (struct dns_decoder_context *)cb_arg;
    uint64_t inner_flag = 0;

    int ret = session_is_inner_most(sess, &inner_flag);
    if (0 == ret) {
        return 0;
    }

    enum session_addr_type sess_addr_type = SESSION_ADDR_TYPE_UNKNOWN;
    struct session_addr *sess_addr = session_get0_addr(sess, &sess_addr_type);

    if (sess_addr_type == SESSION_ADDR_TYPE_IPV4_TCP ||
        sess_addr_type == SESSION_ADDR_TYPE_IPV4_UDP) {
		if (ntohs(sess_addr->ipv4.dport) != 53 &&
			ntohs(sess_addr->ipv4.sport) != 53) {
			return 0;
		}
    } else if (sess_addr_type == SESSION_ADDR_TYPE_IPV6_TCP ||
               sess_addr_type == SESSION_ADDR_TYPE_IPV6_UDP) {
		if (ntohs(sess_addr->ipv6.dport) != 53 &&
			ntohs(sess_addr->ipv6.sport) != 53) {
			return 0;
		}
    } else {
        return 0;
    }

    if (session_get_type(sess) == SESSION_TYPE_TCP) {
        // tcp
    	ret = tcp_dns_decoder_entry(sess, events, pkt, ctx);
    } else {
        // udp
    	ret = udp_dns_decoder_entry(sess, events, pkt, ctx);
    }

    return ret;
}

void *dns_decoder_init(struct stellar *st)
{
    int plugin_id = -1;
    int topic_id = -1;
    int ex_data_idx = -1;

    struct dns_decoder_context *ctx = CALLOC(struct dns_decoder_context, 1);

    ctx->st = st;
    ex_data_idx = stellar_session_get_ex_new_index(st, "DNS_DECODER",
                                                   dns_decoder_ex_data_free,
                                                   NULL);
    if (ex_data_idx < 0) {
        goto failed;
    }
    ctx->ex_data_idx = ex_data_idx;

    plugin_id = stellar_plugin_register(st, SESS_EV_TCP | SESS_EV_UDP,
                                        dns_decoder_entry, ctx);
    if (plugin_id < 0) {
        goto failed;
    }
    ctx->plugin_id = plugin_id;

    topic_id = session_mq_get_topic_id(st, dns_decoder_topic);
    if (topic_id < 0) {
        topic_id = session_mq_create_topic(st, dns_decoder_topic,
                                           dns_message_free, NULL);
    }
    ctx->topic_id = topic_id;

    printf("dns_decoder_init: ex_data_idx:%d, plugin_id:%d, topic_id:%d\n",
           ctx->ex_data_idx, ctx->plugin_id, ctx->topic_id);

    return ctx;

failed:
    _dns_decoder_context_free(ctx);
    return NULL;
}

void dns_decoder_exit(void *decoder_ctx)
{
    if (NULL == decoder_ctx) {
        return;
    }

    struct dns_decoder_context *ctx = (struct dns_decoder_context *)decoder_ctx;

    _dns_decoder_context_free(ctx);
}