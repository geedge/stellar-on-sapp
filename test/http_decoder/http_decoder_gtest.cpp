/*
**********************************************************************************************
*	File: http_decoder_gtest.cpp
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

#include "../../src/http_decoder/http_decoder.h"

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

#define MAX_KEY_STR_LEN 2048

static int g_result_count = 1;
int g_header_count = 1;
int g_req_exdata_idx = 0;
int g_res_exdata_idx = 0;
int g_topic_id = 0;


#if 1
void output_http_req_line(struct http_request_line *req_line)
{
    char tmp_str[MAX_KEY_STR_LEN] = {0};
    snprintf(tmp_str, req_line->method.str_len + 1, "%s", req_line->method.str);
    printf("req_method:%s\n", tmp_str);

    memset(tmp_str, 0, sizeof(tmp_str));
    snprintf(tmp_str, req_line->uri.str_len + 1, "%s", req_line->uri.str);
    printf("req_uri:%s\n", tmp_str);

    memset(tmp_str, 0, sizeof(tmp_str));
    snprintf(tmp_str, req_line->version.str_len + 1, "%s", req_line->version.str);
    printf("req_version:%s\n", tmp_str);
}

void output_http_res_line(struct http_response_line *res_line)
{
    char tmp_str[MAX_KEY_STR_LEN] = {0};
    snprintf(tmp_str, res_line->version.str_len + 1, "%s", res_line->version.str);
    printf("res_version:%s\n", tmp_str);

    memset(tmp_str, 0, sizeof(tmp_str));
    snprintf(tmp_str, res_line->status.str_len + 1, "%s", res_line->status.str);
    printf("res_status:%s\n", tmp_str);
}

void output_http_header(struct http_header *header)
{
    printf("<%s:%s>\n", header->key.str, header->val.str);
}
#endif

void
output_http_body(struct hstring *body, int decompress_flag)
{
    int counter = 0;

    if (1 == decompress_flag) {
        printf("\n\n----------------decompress body len:%zu---------------\n",
              body->str_len);
    } else {
        printf("\n\n----------------raw body len:%zu---------------\n",
               body->str_len);
    }

    for (size_t i = 0; i < body->str_len; i++) {
        if (counter % 16 == 0) {
            printf("\n");
        }
        printf("%02x ", (unsigned char)body->str[i]);
        counter++;
    }
    printf("\n");
}

int
http_field_to_json(cJSON *object, const char *key, char *val, size_t val_len)
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

void
req_line_to_json(cJSON *ctx, struct http_request_line *req_line)
{
    http_field_to_json(ctx, "method", req_line->method.str,
                       req_line->method.str_len);
    http_field_to_json(ctx, "uri", req_line->uri.str, req_line->uri.str_len);
    http_field_to_json(ctx, "req_version", req_line->version.str,
                       req_line->version.str_len);

    cJSON_AddNumberToObject(ctx, "major_version", req_line->major_version);
    cJSON_AddNumberToObject(ctx, "minor_version", req_line->minor_version);
}

void
res_line_to_json(cJSON *ctx, struct http_response_line *res_line)
{
    http_field_to_json(ctx, "res_version", res_line->version.str,
                       res_line->version.str_len);
    http_field_to_json(ctx, "res_status", res_line->status.str,
                       res_line->status.str_len);

    cJSON_AddNumberToObject(ctx, "major_version", res_line->major_version);
    cJSON_AddNumberToObject(ctx, "minor_version", res_line->minor_version);
    cJSON_AddNumberToObject(ctx, "status_code", res_line->status_code);
}

void
http_header_to_json(cJSON *ctx, struct http_header *header)
{
    char key[MAX_KEY_STR_LEN] = {0};

    memcpy(key, header->key.str, header->key.str_len);

    if (cJSON_HasObjectItem(ctx, key) == FALSE) {
        http_field_to_json(ctx, key, header->val.str, header->val.str_len);
    } else {
        //ctx already has the key, so rename key by key%d
        char new_key[MAX_KEY_STR_LEN] = {0};
        sprintf(new_key, "%s%d", key, g_header_count++);
        http_field_to_json(ctx, new_key, header->val.str, header->val.str_len);
    }
}

static int
http_decoder_test_entry(struct session *sess, int topic_id, const void *data,
    void *cb_arg)
{
    int exdata_idx = 0;
    struct http_request_line req_line = {0};
    struct http_response_line res_line = {0};
    struct http_header header = {0};
    struct hstring body = {0};
    struct http_message *msg = (struct http_message *)data; 
    enum http_message_type msg_type = http_message_type(msg);
    
    if (msg_type == HTTP_MESSAGE_REQ_LINE ||
        msg_type == HTTP_MESSAGE_REQ_HEADER) {
        exdata_idx = g_req_exdata_idx;
    } else {
        exdata_idx = g_res_exdata_idx;
    }

    cJSON *json = (cJSON *)session_get_ex_data(sess, exdata_idx);

    if (msg_type == HTTP_MESSAGE_REQ_BODY ||
        msg_type == HTTP_MESSAGE_RES_BODY) {
        goto next;
    }
    
    if (NULL == json) {
        json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "Tuple4", session_get0_readable_addr(sess));
        session_set_ex_data(sess, exdata_idx, json);
    }

next:
    switch (msg_type) {
    case HTTP_MESSAGE_REQ_LINE:
        http_message_get_request_line(msg, &req_line);
        req_line_to_json(json, &req_line);
        break;
    case HTTP_MESSAGE_REQ_HEADER:
        while (http_message_request_header_next(msg, &header) > 0) {
            http_header_to_json(json, &header);
            // output_http_header(&header);
        }
        g_header_count = 1;
        break;
    case HTTP_MESSAGE_REQ_BODY:
        http_message_get_request_raw_body(msg, &body);
        // output_http_body(&body, 0);

        http_message_get_request_decompress_body(msg, &body);
        // output_http_body(&body, 1);
        break;
    case HTTP_MESSAGE_RES_LINE:
        http_message_get_response_line(msg, &res_line);
        res_line_to_json(json, &res_line);
        break;
    case HTTP_MESSAGE_RES_HEADER:
        while (http_message_response_header_next(msg, &header) > 0) {
            http_header_to_json(json, &header);
            // output_http_header(&header);
        }
        g_header_count = 1;
        break;
    case HTTP_MESSAGE_RES_BODY:
        http_message_get_response_raw_body(msg, &body);
        // output_http_body(&body, 0);

        http_message_get_response_decompress_body(msg, &body);
        // output_http_body(&body, 1);
        break;
    default:
        break;
    }

    char result_name[MAX_KEY_STR_LEN] = {0};
    if (msg_type == HTTP_MESSAGE_REQ_HEADER) {
        sprintf(result_name, "HTTP_DECODER_RESULT_%d", g_result_count);
        commit_test_result_json(json, result_name);
        // printf("req json:%s\n", cJSON_Print(ctx));
        session_set_ex_data(sess, exdata_idx, NULL);
        g_result_count++;
    }

    if (msg_type == HTTP_MESSAGE_RES_HEADER) {
        sprintf(result_name, "HTTP_DECODER_RESULT_%d", g_result_count);
        commit_test_result_json(json, result_name);
        // printf("res json:%s\n", cJSON_Print(ctx));
        session_set_ex_data(sess, exdata_idx, NULL);
        g_result_count++;
    }

    return 0;
}

void
http_decoder_test_exdata_free(struct session *sess, int idx, void *ex_ptr,
    void *arg)
{
    if (ex_ptr != NULL) {
        cJSON_Delete((cJSON *)ex_ptr);
    }
}

extern "C" void *http_decoder_test_init(struct stellar *st)
{
    g_req_exdata_idx =
        stellar_session_get_ex_new_index(st, "HTTP_DECODER_REQ_TEST",
                                         http_decoder_test_exdata_free,
                                         NULL);
    if (g_req_exdata_idx < 0) {
        printf("[%s:%d]: can't get http_decoder req exdata index !!!\n",
               __FUNCTION__, __LINE__);	
		exit(-1); 
    }

    g_res_exdata_idx =
        stellar_session_get_ex_new_index(st, "HTTP_DECODER_RES_TEST",
                                         http_decoder_test_exdata_free,
                                         NULL);
    if (g_res_exdata_idx < 0) {
        printf("[%s:%d]: can't get http_decoder res exdata index !!!\n",
               __FUNCTION__, __LINE__);	
		exit(-1); 
    }

    g_topic_id = session_mq_get_topic_id(st, "HTTP_DECODER_MESSAGE");
    if (g_topic_id < 0) {
        printf("[%s:%d]: can't get http_decoder topic id !!!\n",
               __FUNCTION__, __LINE__);	
		exit(-1); 
    }

    session_mq_subscribe_topic(st, g_topic_id, http_decoder_test_entry, NULL);
    printf("http_decoder_test_init OK!\n");	

    return NULL;

}

extern "C" void http_decoder_test_exit(void *test_ctx)
{
    if (test_ctx != NULL) {
        FREE(test_ctx);
    }
    
    printf("http_decoder_test_exit OK!\n");
}