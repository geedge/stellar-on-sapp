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

static int g_result_count = 1;
int g_topic_id = 0;

typedef void rr_printer_fn(uint8_t *in_data, size_t in_data_len,
                           char *out_data, size_t out_data_len);

struct rr_printer {
    int rr_type;
    rr_printer_fn *fn;
};

void print_rr_a(uint8_t *in_data, size_t in_data_len,
                char *out_data, size_t out_data_len)
{
    inet_ntop(AF_INET, in_data, out_data, out_data_len);
}

void print_rr_aaaa(uint8_t *in_data, size_t in_data_len,
                   char *out_data, size_t out_data_len)
{
    inet_ntop(AF_INET6, in_data, out_data, out_data_len);
}

void print_rr_str(uint8_t *in_data, size_t in_data_len,
                char *out_data, size_t out_data_len)
{
    memcpy(out_data, in_data, out_data_len);
}

void print_rr_soa(uint8_t *in_data, size_t in_data_len,
                  char *out_data, size_t out_data_len)
{
    memcpy(out_data, in_data, out_data_len);
}

struct rr_printer rr_printers[] = {
    {-1, NULL},
    {DNS_RR_TYPE_A, print_rr_a},
    {DNS_RR_TYPE_NS, print_rr_str},
    {DNS_RR_TYPE_MD, NULL},
    {DNS_RR_TYPE_MF, NULL},
    {DNS_RR_TYPE_CNAME, NULL},
    {DNS_RR_TYPE_SOA, print_rr_soa},
    {DNS_RR_TYPE_MB, NULL},
    {DNS_RR_TYPE_MG, NULL},
    {DNS_RR_TYPE_MR, NULL},
    {DNS_RR_TYPE_NULL, NULL},
    {DNS_RR_TYPE_WKS, NULL},
    {DNS_RR_TYPE_PTR, NULL},
    {DNS_RR_TYPE_HINFO, NULL},
    {DNS_RR_TYPE_MINFO, NULL},
    {DNS_RR_TYPE_MX, print_rr_str},
    {DNS_RR_TYPE_AAAA, print_rr_aaaa}
};

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
}

static void rr_data_to_buff(struct dns_rr *dns_rr, char *buff, size_t buff_len)
{
    char tmp_buff[1024] = {0};

    switch (dns_rr->type) {
    case DNS_RR_TYPE_CNAME:
        memcpy(buff, dns_rr->rdata.cname, buff_len);
        break;
    case DNS_RR_TYPE_HINFO:
        sprintf(tmp_buff, "%c%c%s%s", dns_rr->rdata.hinfo.os_len,
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
    case DNS_RR_TYPE_MINFO:
        sprintf(tmp_buff, "%s%s", dns_rr->rdata.minfo.rmailbx,
                dns_rr->rdata.minfo.emailbx);
        memcpy(buff, tmp_buff, buff_len);
        break;
    case DNS_RR_TYPE_SOA:
        sprintf(tmp_buff, "%s%s%u%u%u%u%u", dns_rr->rdata.soa.mname,
                dns_rr->rdata.soa.rname, dns_rr->rdata.soa.serial,
                dns_rr->rdata.soa.refresh, dns_rr->rdata.soa.retry,
                dns_rr->rdata.soa.expire, dns_rr->rdata.soa.minimum);
        memcpy(buff, tmp_buff, buff_len);
        break;
    case DNS_RR_TYPE_A:
        inet_ntop(AF_INET, dns_rr->rdata.a, tmp_buff, 1024);
        memcpy(buff, tmp_buff, strlen(tmp_buff));
        break;
    }
}

static void dns_response_to_json(cJSON *ctx, struct dns_response *response)
{
    char key[64] = {0};
    char buff[64] = {0};

    sprintf(buff, "%s", "response");
    dns_field_to_json(ctx, "dns_type", buff, strlen(buff));

    size_t printer_num = sizeof(rr_printers) / sizeof(struct rr_printer);

    for (size_t i = 0; i < response->n_answer_rr; i++) {
        memset(key, 0, sizeof(key));
        memset(buff, 0, sizeof(buff));

        sprintf(key, "ans_rr%zu", i);
        rr_data_to_buff(&response->answer_rr[i], buff, sizeof(buff));
        dns_field_to_json(ctx, key, buff, sizeof(buff));
    }

    for (size_t i = 0; i < response->n_authority_rr; i++) {
        memset(key, 0, sizeof(key));
        memset(buff, 0, sizeof(buff));

        sprintf(key, "auth_rr%zu", i);
        rr_data_to_buff(&response->authority_rr[i], buff, sizeof(buff));
        dns_field_to_json(ctx, key, buff, strlen(buff));
    }

    for (size_t i = 0; i < response->n_additional_rr; i++) {
        memset(key, 0, sizeof(key));
        memset(buff, 0, sizeof(buff));

        sprintf(key, "addt_rr%zu", i);
        rr_data_to_buff(&response->additional_rr[i], buff, sizeof(buff));
        dns_field_to_json(ctx, key, buff, strlen(buff));
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