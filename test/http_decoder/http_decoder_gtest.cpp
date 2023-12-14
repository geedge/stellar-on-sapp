/*
 *  author:yangwei
 *  create time:2021-8-21
 *
 */



#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#include "../../src/http_decoder/http_decoder.h"

#ifdef __cplusplus
extern "C"
{


#include "cJSON.h"
#include "MESA_prof_load.h"

#include "stellar/utils.h"
#include "stellar/stellar.h"
#include "stellar/session_exdata.h"
#include "stellar/session_mq.h"

int commit_test_result_json(cJSON *node, const char *name);
}
#endif

#include "MESA/stream.h"

static int g_result_count = 1;
int g_req_exdata_idx = 0;
int g_res_exdata_idx = 0;

void output_http_req_line(struct http_request_line *req_line)
{
    char tmp_str[2048] = {0};
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
    char tmp_str[2048] = {0};
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

void output_http_req_body(struct hstring *body)
{

}

void output_http_res_body(struct hstring *body)
{

}


int http_field_add_to_json(cJSON *object, const char *name, char *value, char length)
{
    if (value != NULL && length > 0) {
        char *tmp = (char *)calloc(1, length + 1);
        memcpy(tmp, value, length);
        cJSON_AddStringToObject(object, name, tmp);
        free(tmp);
        tmp = NULL;
    }

        return 0;
}

void req_line_to_json(cJSON *ctx, struct http_request_line *req_line)
{
    http_field_add_to_json(ctx, "method", req_line->method.str,
                           req_line->method.str_len);
    http_field_add_to_json(ctx, "uri", req_line->uri.str,
                           req_line->uri.str_len);
    http_field_add_to_json(ctx, "req_version", req_line->version.str,
                           req_line->version.str_len);
    cJSON_AddNumberToObject(ctx, "major_version", req_line->major_version);
    cJSON_AddNumberToObject(ctx, "minor_version", req_line->minor_version);
}

void res_line_to_json(cJSON *ctx, struct http_response_line *res_line)
{
    http_field_add_to_json(ctx, "res_version", res_line->version.str,
                           res_line->version.str_len);
    http_field_add_to_json(ctx, "res_status", res_line->status.str,
                                 res_line->status.str_len);            
    cJSON_AddNumberToObject(ctx, "major_version", res_line->major_version);
    cJSON_AddNumberToObject(ctx, "minor_version", res_line->minor_version);
    cJSON_AddNumberToObject(ctx, "status_code", res_line->status_code);
}

void http_header_to_json(cJSON *ctx, struct http_header *header)
{
    char key[256] = {0};
    snprintf(key, header->key.str_len + 1, "%s", header->key.str);

    http_field_add_to_json(ctx, key, header->val.str, header->val.str_len);
}

static int http_decoder_test_entry(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
    struct http_message *msg = (struct http_message *)data;
    struct http_request_line req_line = {0};
    struct http_response_line res_line = {0};
    struct http_header header = {0};
    struct hstring body = {0};
    int exdata_idx = 0;

    enum http_message_type msg_type = http_message_type(msg);
    
    if (msg_type == HTTP_MESSAGE_REQ_LINE ||
        msg_type == HTTP_MESSAGE_REQ_HEADER_END ||
        msg_type == HTTP_MESSAGE_REQ_BODY) {
        exdata_idx = g_req_exdata_idx;
    } else {
        exdata_idx = g_res_exdata_idx;
    }

    cJSON *ctx = (cJSON *)session_get_ex_data(sess, exdata_idx);
    if (NULL == ctx) {
        ctx = cJSON_CreateObject();
        cJSON_AddStringToObject(ctx, "Tuple4", session_get0_readable_addr(sess));
        session_set_ex_data(sess, exdata_idx, ctx);
    }

    switch (msg_type) {
    case HTTP_MESSAGE_REQ_LINE:
        http_message_get_request_line(msg, &req_line);
        //output_http_req_line(&req_line);
        req_line_to_json(ctx, &req_line);
        break;
    case HTTP_MESSAGE_REQ_HEADER_END:
        while (http_message_request_header_next(msg, &header) > 0) {
            http_header_to_json(ctx, &header);
        }
        break;
    case HTTP_MESSAGE_REQ_BODY:
        http_message_get_request_raw_body(msg, &body);
        output_http_req_body(&body);
        break;
    case HTTP_MESSAGE_RES_LINE:
        http_message_get_response_line(msg, &res_line);
        res_line_to_json(ctx, &res_line);
        break;
    case HTTP_MESSAGE_RES_HEADER_END:
        while (http_message_response_header_next(msg, &header) > 0) {
            http_header_to_json(ctx, &header);
        }
        break;
    case HTTP_MESSAGE_RES_BODY:
        http_message_get_response_raw_body(msg, &body);
        output_http_res_body(&body);
        break;
    default:
        break;
    }

    char result_name[128] = {0};
    if (msg_type == HTTP_MESSAGE_REQ_HEADER_END) {
        sprintf(result_name, "HTTP_DECODER_RESULT_%d", g_result_count);
        commit_test_result_json(ctx, result_name);
        session_set_ex_data(sess, exdata_idx, NULL);
        g_result_count++;
    }

    if (msg_type == HTTP_MESSAGE_RES_HEADER_END) {
        sprintf(result_name, "HTTP_DECODER_RESULT_%d", g_result_count);
        commit_test_result_json(ctx, result_name);
        session_set_ex_data(sess, exdata_idx, NULL);
        g_result_count++;
    }

    return 0;
}

extern "C" void *http_decoder_test_init(struct stellar *st)
{
    g_req_exdata_idx = stellar_session_get_ex_new_index(st, "HTTP_DECODER_REQ_TEST", NULL, NULL);
    if (g_req_exdata_idx < 0) {
        printf("http_decoder_test_init: can't get http_decoder req exdata index !!!\n");	
		exit(-1); 
    }

    g_res_exdata_idx = stellar_session_get_ex_new_index(st, "HTTP_DECODER_RES_TEST", NULL, NULL);
    if (g_res_exdata_idx < 0) {
        printf("http_decoder_test_init: can't get http_decoder res exdata index !!!\n");	
		exit(-1); 
    }

    int topic_id = session_mq_get_topic_id(st, "HTTP_DECODER_MESSAGE");
    if (topic_id < 0) {
        printf("http_decoder_test_init: can't get http_decoder topic id !!!\n");	
		exit(-1); 
    }

    session_mq_subscribe_topic(st, topic_id, http_decoder_test_entry, NULL);
    printf("http_decoder_test_init OK!\n");	

    return NULL;
}

extern "C" void http_decoder_test_exit(void *ctx)
{
    if (NULL == ctx) {
        return;
    }

    FREE(ctx);
    printf("http_decoder_test_exit OK!\n");
}