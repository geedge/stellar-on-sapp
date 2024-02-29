/*
**********************************************************************************************
*	File: dns_decoder_gtest.cpp
*   Description: 
*	Authors: Liu WenTan <liuwentan@geedgenetworks.com>
*	Date:    2023-12-15
*   Copyright: (c) Since 2023 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <arpa/inet.h>

#include "../../src/dns_decoder/dns_decoder.h"

#ifdef __cplusplus
extern "C"
{

#include "cJSON.h"

#include "stellar/utils.h"
#include "stellar/stellar.h"
#include "stellar/session_exdata.h"
#include "stellar/session_mq.h"

int commit_test_result_json(cJSON *node, const char *name);
}
#endif

#define DD_IPV4 4
#define DD_IPV6 6

#define MAX_KEY_STR_LEN 2048
#define BUFF_LEN 2048

static int g_result_count = 1;
int g_topic_id = 0;

static const char *rr_type_int2str(enum dns_rr_type type)
{
    switch (type) {
        case DNS_RR_TYPE_A:
            return "A";
        case DNS_RR_TYPE_NS:
            return "NS";
        case DNS_RR_TYPE_MD:
            return "MD";
        case DNS_RR_TYPE_MF:
            return "MF";
        case DNS_RR_TYPE_CNAME:
            return "CNAME";
        case DNS_RR_TYPE_SOA:
            return "SOA";
        case DNS_RR_TYPE_MB:
            return "MB";
        case DNS_RR_TYPE_MG:
            return "MG";
        case DNS_RR_TYPE_MR:
            return "MR";
        case DNS_RR_TYPE_NULL:
            return "NULL";
        case DNS_RR_TYPE_WKS:
            return "WKS";
        case DNS_RR_TYPE_PTR:
            return "PTR";
        case DNS_RR_TYPE_HINFO:
            return "HINFO";
        case DNS_RR_TYPE_MINFO:
            return "MINFO";
        case DNS_RR_TYPE_MX:
            return "MX";
        case DNS_RR_TYPE_TXT:
            return "TXT";
        case DNS_RR_TYPE_RP:
            return "RP";
        case DNS_RR_TYPE_ISDN:
            return "ISDN";
        case DNS_RR_TYPE_AAAA:
            return "AAAA";
        case DNS_RR_TYPE_SRV:
            return "SRV";
        case DNS_RR_TYPE_DNAME:
            return "DNAME";
        case DNS_RR_TYPE_OPT:
            return "OPT";
        case DNS_RR_TYPE_DS:
            return "DS";
        case DNS_RR_TYPE_RRSIG:
            return "RRSIG";
        case DNS_RR_TYPE_NSEC:
            return "NSEC";
        case DNS_RR_TYPE_DNSKEY:
            return "DNSKEY";
        case DNS_RR_TYPE_NSEC3:
            return "NSEC3";
        case DNS_RR_TYPE_NSEC3PARAM:
            return "NSEC3PARAM";
        case DNS_RR_TYPE_HTTPS:
            return "HTTPS";
        case DNS_RR_TYPE_AXFR:
            return "AXFR";
        case DNS_RR_TYPE_MAILB:
            return "MAILB";
        case DNS_RR_TYPE_MAILA:
            return "MAILA";
        case DNS_RR_TYPE_ANY:
            return "ANY";
        case DNS_RR_TYPE_DLV:
            return "DLV";
        case DNS_RR_TYPE_UNKNOWN:
            return "UNKNOWN";
        default:
            return "UNKNOWN";
        }
}

static int
dns_field_to_json(cJSON *object, const char *key, char *val, size_t val_len)
{
    if (NULL == object || NULL == key || NULL == val || 0 == val_len) {
        return -1;
    }

    char *tmp = CALLOC(char, val_len + 1);
    memcpy(tmp, val, val_len);
    cJSON_AddStringToObject(object, key, tmp);
    FREE(tmp);
    
    return 0;
}

static void dns_query_to_json(cJSON *ctx, struct dns_query *query)
{
    char buff[64] = {0};
    sprintf(buff, "%x", query->transaction_id);
    dns_field_to_json(ctx, "transaction_id", buff, strlen(buff));

    memset(buff, 0, sizeof(buff));
    sprintf(buff, "%s", "query");
    dns_field_to_json(ctx, "dns_type", buff, strlen(buff));

    memset(buff, 0, sizeof(buff));
    sprintf(buff, "%s", query->query_question.qname);
    dns_field_to_json(ctx, "qname", buff, strlen(buff));

    memset(buff, 0, sizeof(buff));
    sprintf(buff, "%s", rr_type_int2str(query->query_question.qtype));
    dns_field_to_json(ctx, "qtype", buff, strlen(buff));

    memset(buff, 0, sizeof(buff));
    sprintf(buff, "%u", query->flag.qr);
    dns_field_to_json(ctx, "flag_qr", buff, strlen(buff));

    memset(buff, 0, sizeof(buff));
    sprintf(buff, "%u", query->flag.opcode);
    dns_field_to_json(ctx, "flag_opcode", buff, strlen(buff));

    memset(buff, 0, sizeof(buff));
    sprintf(buff, "%u", query->flag.rd);
    dns_field_to_json(ctx, "flag_rd", buff, strlen(buff));
}

static void rr_data_to_buff(struct dns_rr *dns_rr, char *buff, size_t buff_len)
{
    char tmp_buff[BUFF_LEN] = {0};

    switch (dns_rr->type) {
    case DNS_RR_TYPE_CNAME:
        memcpy(buff, dns_rr->rdata.cname, buff_len);
        break;
    case DNS_RR_TYPE_HINFO:
        sprintf(tmp_buff, "%c %c %s %s", dns_rr->rdata.hinfo.os_len,
                dns_rr->rdata.hinfo.cpu_len, dns_rr->rdata.hinfo.cpu,
                dns_rr->rdata.hinfo.os);
        memcpy(buff, tmp_buff, buff_len);
        break;
    case DNS_RR_TYPE_MB:
        memcpy(buff, dns_rr->rdata.mb, buff_len);
        break;
    case DNS_RR_TYPE_MD:
        memcpy(buff, dns_rr->rdata.md, buff_len);
        break;
    case DNS_RR_TYPE_MF:
        memcpy(buff, dns_rr->rdata.mf, buff_len);
        break;
    case DNS_RR_TYPE_MG:
        memcpy(buff, dns_rr->rdata.mg, buff_len);
        break;
    case DNS_RR_TYPE_MR:
        memcpy(buff, dns_rr->rdata.mr, buff_len);
        break;
    case DNS_RR_TYPE_NS:
        memcpy(buff, dns_rr->rdata.ns, buff_len);
        break;
    case DNS_RR_TYPE_MX:
        sprintf(tmp_buff, "%u %s", dns_rr->rdata.mx.preference,
                dns_rr->rdata.mx.exchange);
        memcpy(buff, tmp_buff, buff_len);
        break;
    case DNS_RR_TYPE_MINFO:
        sprintf(tmp_buff, "%s %s", dns_rr->rdata.minfo.rmailbx,
                dns_rr->rdata.minfo.emailbx);
        memcpy(buff, tmp_buff, buff_len);
        break;
    case DNS_RR_TYPE_SOA:
        sprintf(tmp_buff, "%s %s %u %u %u %u %u", dns_rr->rdata.soa.mname,
                dns_rr->rdata.soa.rname, dns_rr->rdata.soa.serial,
                dns_rr->rdata.soa.refresh, dns_rr->rdata.soa.retry,
                dns_rr->rdata.soa.expire, dns_rr->rdata.soa.minimum);
        memcpy(buff, tmp_buff, buff_len);
        break;
    case DNS_RR_TYPE_A:
        inet_ntop(AF_INET, dns_rr->rdata.a, tmp_buff, BUFF_LEN);
        memcpy(buff, tmp_buff, strlen(tmp_buff));
        break;
    case DNS_RR_TYPE_AAAA:
        inet_ntop(AF_INET6, dns_rr->rdata.aaaa, tmp_buff, BUFF_LEN);
        memcpy(buff, tmp_buff, strlen(tmp_buff));
        break;
    }
}

static void dns_response_to_json(cJSON *ctx, struct dns_response *response)
{
    char key[BUFF_LEN] = {0};
    char buff[BUFF_LEN] = {0};

    sprintf(buff, "%s", "response");
    dns_field_to_json(ctx, "dns_type", buff, strlen(buff));

    memset(buff, 0, sizeof(buff));
    sprintf(buff, "%s", response->query.query_question.qname);
    dns_field_to_json(ctx, "qname", buff, strlen(buff));

    memset(buff, 0, sizeof(buff));
    sprintf(buff, "%s", rr_type_int2str(response->query.query_question.qtype));
    dns_field_to_json(ctx, "qtype", buff, strlen(buff));

    memset(buff, 0, sizeof(buff));
    sprintf(buff, "%u", response->query.flag.qr);
    dns_field_to_json(ctx, "flag_qr", buff, strlen(buff));

    memset(buff, 0, sizeof(buff));
    sprintf(buff, "%u", response->query.flag.opcode);
    dns_field_to_json(ctx, "flag_opcode", buff, strlen(buff));

    memset(buff, 0, sizeof(buff));
    sprintf(buff, "%u", response->query.flag.rd);
    dns_field_to_json(ctx, "flag_rd", buff, strlen(buff));

    for (size_t i = 0; i < response->n_answer_rr; i++) {
        memset(key, 0, sizeof(key));
        memset(buff, 0, sizeof(buff));

        sprintf(key, "ans_rr%zu", i);
        cJSON *obj = cJSON_AddObjectToObject(ctx, key);

        sprintf(buff, "%s", rr_type_int2str(response->answer_rr[i].type));
        dns_field_to_json(obj, "rr_type", buff, strlen(buff));

        memset(buff, 0, sizeof(buff));
        rr_data_to_buff(&response->answer_rr[i], buff, sizeof(buff));
        dns_field_to_json(obj, "rr_data", buff, strlen(buff));
    }

    for (size_t i = 0; i < response->n_authority_rr; i++) {
        memset(key, 0, sizeof(key));
        memset(buff, 0, sizeof(buff));

        sprintf(key, "auth_rr%zu", i);
        cJSON *obj = cJSON_AddObjectToObject(ctx, key);

        sprintf(buff, "%s", rr_type_int2str(response->authority_rr[i].type));
        dns_field_to_json(obj, "rr_type", buff, strlen(buff));

        memset(buff, 0, sizeof(buff));
        rr_data_to_buff(&response->authority_rr[i], buff, sizeof(buff));
        dns_field_to_json(obj, "rr_data", buff, strlen(buff));
    }

    for (size_t i = 0; i < response->n_additional_rr; i++) {
        memset(key, 0, sizeof(key));
        memset(buff, 0, sizeof(buff));

        sprintf(key, "addt_rr%zu", i);
        cJSON *obj = cJSON_AddObjectToObject(ctx, key);

        sprintf(buff, "%s", rr_type_int2str(response->additional_rr[i].type));
        dns_field_to_json(obj, "rr_type", buff, strlen(buff));

        memset(buff, 0, sizeof(buff));
        rr_data_to_buff(&response->additional_rr[i], buff, sizeof(buff));
        dns_field_to_json(obj, "rr_data", buff, strlen(buff));
    }
}

static int
dns_decoder_test_entry(struct session *sess, int topic_id, const void *data,
                       void *cb_arg)
{
    int exdata_idx = 0;
    struct dns_message *msg = (struct dns_message *)data; 

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "Tuple4", session_get0_readable_addr(sess));

    enum dns_message_type type = dns_message_type(msg);
    struct dns_query query;
    struct dns_response response;

    switch (type) {
    case DNS_MESSAGE_QUERY:
        dns_message_get_query(msg, &query);
        dns_query_to_json(json, &query);
        break;
    case DNS_MESSAGE_RESPONSE:
        dns_message_get_response(msg, &response);
        dns_response_to_json(json, &response);
        break;
    default:
        break;
    }

    char result_name[MAX_KEY_STR_LEN] = {0};
    sprintf(result_name, "DNS_DECODER_RESULT_%d", g_result_count);
    commit_test_result_json(json, result_name);
    g_result_count++;

    return 0;
}

static void
dns_decoder_test_exdata_free(struct session *sess, int idx, void *ex_ptr, void *arg)
{
    if (ex_ptr != NULL) {
        cJSON_Delete((cJSON *)ex_ptr);
    }
}

extern "C" void *dns_decoder_test_init(struct stellar *st)
{
    g_topic_id = session_mq_get_topic_id(st, "DNS_DECODER_MESSAGE");
    if (g_topic_id < 0) {
        printf("[%s:%d]: can't get dns_decoder topic id !!!\n",
               __FUNCTION__, __LINE__);	
		exit(-1); 
    }

    session_mq_subscribe_topic(st, g_topic_id, dns_decoder_test_entry, NULL);
    printf("dns_decoder_test_init OK!\n");	

    return NULL;

}

extern "C" void dns_decoder_test_exit(void *test_ctx)
{
    if (test_ctx != NULL) {
        FREE(test_ctx);
    }
    
    printf("dns_decoder_test_exit OK!\n");
}