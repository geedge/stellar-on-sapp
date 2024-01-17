/*
**********************************************************************************************
*	File: http_decoder.c
*   Description: 
*	Authors: Liu WenTan <liuwentan@geedgenetworks.com>
*	Date:    2024-01-10
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "stellar/utils.h"
#include "stellar/session.h"
#include "stellar/session_mq.h"
#include "stellar/session_exdata.h"
#include "http_decoder.h"
#include "http_decoder_half.h"
#include "http_decoder_table.h"
#include "http_decoder_result_queue.h"
#include "llhttp.h"
#include "fieldstat/fieldstat_easy.h"

#define HTTP_IDENTIFY_LEN 16
#define HD_RESULT_QUEUE_SIZE 16
#define HD_IS_CACHE_BODY 1

const char *http_decoder_topic = "HTTP_DECODER_MESSAGE";
const char *fs_file_name = "http_decoder.fs";

/**
 * NOTE: http_message don't have the ownership of data
*/
struct http_message {
	enum http_message_type type;
	struct http_decoder_half_data *data;
};

struct http_decoder {
	struct http_decoder_half *c2s_half;
	struct http_decoder_half *s2c_half;
};

struct http_decoder_exdata {
    struct http_decoder_result_queue *queue;
    struct http_decoder *decoder;
};

struct http_decoder_context {
    int plugin_id;
	int topic_id;
    int ex_data_idx;
    int fs_incoming_bytes_id;
    int fs_incoming_pkts_id;
    int fs_incoming_trans_id;
    struct fieldstat_easy *fse;
    struct stellar *st;
};

static void http_event_handler(enum http_event event,
                               struct http_decoder_half_data **data,
                               struct http_event_context *ev_ctx)
{
    assert(ev_ctx);

    struct http_decoder_result_queue *queue = ev_ctx->ref_queue;
    struct http_message *msg = NULL;
    struct http_decoder_half_data *half_data = NULL;
    int ret = 0;

    switch (event) {
    case HTTP_EVENT_REQ_INIT:
        half_data = http_decoder_result_queue_peek_req(queue);
        if (half_data != NULL) {
            http_decoder_result_queue_inc_req_index(queue);
        }

        half_data = http_decoder_result_queue_peek_req(queue);
        if (half_data != NULL) {
            half_data = http_decoder_result_queue_pop_req(queue);
            http_decoder_half_data_free(half_data);
            half_data = NULL;
        }

        half_data = http_decoder_half_data_new();
        ret = http_decoder_result_queue_push_req(queue, half_data);
        if (ret < 0) {
            fprintf(stderr, "http_decoder_result_queue_push req failed.");
            http_decoder_half_data_free(half_data);
            half_data = NULL;
        }
        *data = half_data;
        break;
    case HTTP_EVENT_REQ_LINE:
        msg = CALLOC(struct http_message, 1);
	    msg->type = HTTP_MESSAGE_REQ_LINE;
        msg->data = *data;
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_REQ_HDR_END:
        msg = CALLOC(struct http_message, 1);
	    msg->type = HTTP_MESSAGE_REQ_HEADER;
        msg->data = *data;
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_REQ_BODY_BEGIN:
        break;
    case HTTP_EVENT_REQ_BODY_DATA:
        msg = CALLOC(struct http_message, 1);
	    msg->type = HTTP_MESSAGE_REQ_BODY;
        msg->data = *data;
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_REQ_BODY_END:
        break;
    case HTTP_EVENT_REQ_END:
        http_decoder_result_queue_inc_req_index(queue);
		half_data = http_decoder_result_queue_pop_req(queue);
        if (half_data != NULL) {
            http_decoder_half_data_free(half_data);
            half_data = NULL;
        }
        break;
    case HTTP_EVENT_RES_INIT:
        half_data = http_decoder_result_queue_peek_res(queue);
        if (half_data != NULL) {
            http_decoder_result_queue_inc_res_index(queue);
        }

        half_data = http_decoder_result_queue_peek_res(queue);
        if (half_data != NULL) {
            half_data = http_decoder_result_queue_pop_res(queue);
            http_decoder_half_data_free(half_data);
            half_data = NULL;
        }

        half_data = http_decoder_half_data_new();
        ret = http_decoder_result_queue_push_res(queue, half_data);
        if (ret < 0) {
            fprintf(stderr, "http_decoder_result_queue_push res failed.");
            http_decoder_half_data_free(half_data);
            half_data = NULL;
        }
        *data = half_data;
        break;
    case HTTP_EVENT_RES_LINE:
        msg = CALLOC(struct http_message, 1);
	    msg->type = HTTP_MESSAGE_RES_LINE;
        msg->data = *data;
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_RES_HDR:
        break;
    case HTTP_EVENT_RES_HDR_END:
        msg = CALLOC(struct http_message, 1);
	    msg->type = HTTP_MESSAGE_RES_HEADER;
        msg->data = *data;
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg); 
        break;
    case HTTP_EVENT_RES_BODY_BEGIN:
        break;
    case HTTP_EVENT_RES_BODY_DATA:
        msg = CALLOC(struct http_message, 1);
	    msg->type = HTTP_MESSAGE_RES_BODY;
        msg->data = *data;
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg); 
        break;
    case HTTP_EVENT_RES_BODY_END:
        break;
    case HTTP_EVENT_RES_END:
        http_decoder_result_queue_inc_res_index(queue);
		half_data = http_decoder_result_queue_pop_res(queue);
        if (half_data != NULL) {
            http_decoder_half_data_free(half_data);
            half_data = NULL;
        }
        break;
    default:
        assert(0);
        break;
    }
}

static struct http_decoder *
http_decoder_new(http_event_cb *ev_cb, int is_cache_body)
{
	struct http_decoder *decoder = CALLOC(struct http_decoder, 1);
	assert(decoder);

	decoder->c2s_half = http_decoder_half_new(ev_cb, is_cache_body, HTTP_REQUEST);
	decoder->s2c_half = http_decoder_half_new(ev_cb, is_cache_body, HTTP_RESPONSE);

	return decoder;
}

static void http_decoder_free(struct http_decoder *decoder)
{
    if (NULL == decoder) {
        return;
    }

    if (decoder->c2s_half != NULL) {
        http_decoder_half_free(decoder->c2s_half);
        decoder->c2s_half = NULL;
    }

    if (decoder->s2c_half != NULL) {
        http_decoder_half_free(decoder->s2c_half);
        decoder->s2c_half = NULL;
    }

    FREE(decoder);
}

static struct http_decoder_exdata *http_decoder_exdata_new(size_t queue_size)
{
    struct http_decoder_exdata *ex_data = CALLOC(struct http_decoder_exdata, 1);

    ex_data->decoder = http_decoder_new(http_event_handler, 0);
    ex_data->queue = http_decoder_result_queue_new(HD_RESULT_QUEUE_SIZE);

    return ex_data;
}

static void http_decoder_exdata_free(struct http_decoder_exdata *ex_data)
{
    if (NULL == ex_data) {
        return;
    }

    if (ex_data->decoder != NULL) {
        http_decoder_free(ex_data->decoder);
        ex_data->decoder = NULL;
    }

    if (ex_data->queue != NULL) {
        http_decoder_result_queue_free(ex_data->queue);
        ex_data->queue = NULL;
    }

    FREE(ex_data);
}

static int http_protocol_identify(const char *data, size_t data_len)
{
	llhttp_t parser;
	llhttp_settings_t settings;
	enum llhttp_errno error;

	llhttp_settings_init(&settings);
	llhttp_init(&parser, HTTP_BOTH, &settings);

	error = llhttp_execute(&parser, data, data_len);
	if (error != HPE_OK) {
		return -1;
	}

	return 0;
}

int http_decoder_entry(struct session *sess, int events,
                       const struct packet *pkt, void *cb_arg)
{
    struct http_decoder_context *decoder_ctx = (struct http_decoder_context *)cb_arg;
    size_t payload_len = 0;
    uint64_t inner_flag = 0;

    int ret = session_is_inner_most(sess, &inner_flag);
    if (0 == ret) {
        return 0;
    }

    struct http_decoder_exdata *ex_data =
        session_get_ex_data(sess, decoder_ctx->ex_data_idx);

    if (events & SESS_EV_CLOSING) {
        if (ex_data != NULL) {
            http_decoder_exdata_free(ex_data);
            session_set_ex_data(sess, decoder_ctx->ex_data_idx, NULL);
        }

        return 0;
    }

    const char *payload = session_get0_current_payload(sess, &payload_len);
    
    if (events & SESS_EV_OPENING) {
        assert(ex_data == NULL);

        //If not http, ignore this session
        if (payload_len > 0) {
            size_t http_identify_len = payload_len > HTTP_IDENTIFY_LEN
                                                   ? HTTP_IDENTIFY_LEN
                                                   : payload_len;
            
            ret = http_protocol_identify(payload, http_identify_len);
            if (ret < 0) {
                // ignore this session's event
                struct session_event *s_event =
                    session_get_intrinsic_event(sess, decoder_ctx->plugin_id);

                session_event_assign(s_event, decoder_ctx->st, sess, 0,
                                     http_decoder_entry, decoder_ctx);
                return 0;
            }
        }

        ex_data = http_decoder_exdata_new(HD_RESULT_QUEUE_SIZE);
        session_set_ex_data(sess, decoder_ctx->ex_data_idx, ex_data);
    }

    if (0 == payload_len || NULL == ex_data) {
        return 0;
    }

    int dir = packet_get_direction(pkt);
    if (dir < 0) {
        return -1;
    }

    struct http_decoder_half *cur_half = NULL;
    if (dir == PACKET_DIRECTION_C2S) {
        cur_half = ex_data->decoder->c2s_half;
    } else {
        cur_half = ex_data->decoder->s2c_half;
    }

    http_decoder_half_reinit(cur_half, decoder_ctx->topic_id, ex_data->queue, sess);
    http_decoder_half_parse(cur_half, payload, payload_len);

    int thread_id = session_get_current_thread_id(sess);

    fieldstat_easy_counter_incrby(decoder_ctx->fse, thread_id,
                                  decoder_ctx->fs_incoming_bytes_id,
                                  NULL, 0, payload_len);

    fieldstat_easy_counter_incrby(decoder_ctx->fse, thread_id,
                                  decoder_ctx->fs_incoming_pkts_id,
                                  NULL, 0, 1);

    long long trans_cnt = http_decoder_half_trans_count(cur_half);
    fieldstat_easy_counter_incrby(decoder_ctx->fse, thread_id,
                                  decoder_ctx->fs_incoming_trans_id,
                                  NULL, 0, trans_cnt);

    return 0;
}

static void http_message_free(void *msg, void *cb_arg)
{
	if (NULL == msg) {
		return;
	}

	struct http_message *message = (struct http_message *)msg;
	message->data = NULL; //don't have memory's ownership

	FREE(message);
}

static void _http_decoder_context_free(struct http_decoder_context *ctx)
{
    if (NULL == ctx) {
        return;
    }

    if (ctx->fse != NULL) {
        fieldstat_easy_free(ctx->fse);
        ctx->fse = NULL;
    }

    if (ctx->topic_id >= 0) {
        session_mq_destroy_topic(ctx->st, ctx->topic_id);
        ctx->topic_id = -1;
    }

    FREE(ctx);
}

static void http_decoder_ex_data_free(struct session *s, int idx,
                                      void *ex_data, void *arg)
{
    if (NULL == ex_data) {
        return;
    }

    struct http_decoder_exdata *exdata = (struct http_decoder_exdata *)ex_data;
    http_decoder_exdata_free(exdata);
}

#define FS_OUTPUT_INTERVAL_S 1
void *http_decoder_init(struct stellar *st)
{
    struct http_decoder_context *ctx = CALLOC(struct http_decoder_context, 1);

    ctx->st = st;
    ctx->ex_data_idx = stellar_session_get_ex_new_index(st, "HTTP_DECODER",
														http_decoder_ex_data_free,
                                                        NULL);

    int plugin_id = stellar_plugin_register(st, SESS_EV_TCP|SESS_EV_CLOSING,
                                            http_decoder_entry, ctx);
    if (plugin_id >= 0) {
        ctx->plugin_id = plugin_id;
    }

    int topic_id = session_mq_get_topic_id(st, http_decoder_topic);
    if (topic_id < 0) {
        topic_id = session_mq_create_topic(st, http_decoder_topic,
                                           http_message_free, NULL);
    }
    ctx->topic_id = topic_id;

    int thread_num = stellar_get_worker_thread_num(st);

    ctx->fse = fieldstat_easy_new(thread_num, "http_decoder_statistics", NULL, 0);
    if (NULL == ctx->fse) {
        fprintf(stderr, "fieldstat_easy_new failed.");
        goto failed;
    }

    ctx->fs_incoming_bytes_id =
        fieldstat_easy_register_counter(ctx->fse, "incoming_bytes");
    if (ctx->fs_incoming_bytes_id < 0) {
        fprintf(stderr, "fieldstat_easy_register_counter incoming_bytes failed.");
        goto failed;
    }

    ctx->fs_incoming_trans_id =
        fieldstat_easy_register_counter(ctx->fse, "incoming_trans");
    if (ctx->fs_incoming_trans_id < 0) {
        fprintf(stderr, "fieldstat_easy_register_counter incoming_trans failed.");
        goto failed;
    }

    ctx->fs_incoming_pkts_id =
        fieldstat_easy_register_counter(ctx->fse, "incoming_pkts");
    if (ctx->fs_incoming_pkts_id < 0) {
        fprintf(stderr, "fieldstat_easy_register_counter incoming_pkts failed.");
        goto failed;
    }

    int ret = fieldstat_easy_enable_auto_output(ctx->fse, fs_file_name,
                                                FS_OUTPUT_INTERVAL_S);
    if (ret < 0) {
        fprintf(stderr, "fieldstat_easy_enable_auto_output failed.");
        goto failed;
    }
    sleep(1);

    printf("http_decoder_init: ex_data_idx:%d, plugin_id:%d, topic_id:%d\n",
           ctx->ex_data_idx, ctx->plugin_id, ctx->topic_id);

    return ctx;

failed:
    _http_decoder_context_free(ctx);
    return NULL;
}

void http_decoder_exit(void *decoder_ctx)
{
	if (NULL == decoder_ctx) {
        return;
    }

    struct http_decoder_context *ctx =
        (struct http_decoder_context *)decoder_ctx;

    _http_decoder_context_free(ctx);
}

enum http_message_type http_message_type(struct http_message *msg)
{
	if (NULL == msg) {
		return HTTP_MESSAGE_MAX;
	}

	return msg->type;
}

int http_message_get_request_line(struct http_message *msg,
                                  struct http_request_line *line)
{
	if (NULL == msg || msg->type != HTTP_MESSAGE_REQ_LINE ||
        NULL == line) {
		return -1;
	}

	return http_decoder_half_data_get_request_line(msg->data, line);
}

int http_message_get_response_line(struct http_message *msg,
                                   struct http_response_line *line)
{
	if (NULL == msg || msg->type != HTTP_MESSAGE_RES_LINE ||
        NULL == line) {
		return -1;
	}

	return http_decoder_half_data_get_response_line(msg->data, line);
}

int http_message_get_request_header(struct http_message *msg, struct hstring *key,
	                                struct http_header *hdr_array, size_t array_size)
{
	if (NULL == msg || msg->type != HTTP_MESSAGE_REQ_HEADER ||
        NULL == key || NULL == hdr_array || 0 == array_size) {
		return -1;
	}

	return http_decoder_half_data_get_header(msg->data, key, hdr_array, array_size);
}

int http_message_get_response_header(struct http_message *msg, struct hstring *key,
	                                 struct http_header *hdr_array, size_t array_size)
{
	if (NULL == msg || msg->type != HTTP_MESSAGE_RES_HEADER || NULL == key ||
        NULL == hdr_array || 0 == array_size) {
		return -1;
	}

	return http_decoder_half_data_get_header(msg->data, key, hdr_array, array_size);
}

int http_message_request_header_next(struct http_message *msg,
                                     struct http_header *hdr)
{
    if (NULL == msg || msg->type != HTTP_MESSAGE_REQ_HEADER
        || NULL == hdr) {
		return -1;
	}

    return http_decoder_half_data_iter_header(msg->data, hdr);
}

int http_message_response_header_next(struct http_message *msg,
                                      struct http_header *hdr)
{
    if (NULL == msg || msg->type != HTTP_MESSAGE_RES_HEADER ||
        NULL == hdr) {
		return -1;
	}

    return http_decoder_half_data_iter_header(msg->data, hdr);
}

int http_message_get_request_raw_body(struct http_message *msg,
                                      struct hstring *body)
{
    if (NULL == msg || (msg->type != HTTP_MESSAGE_REQ_BODY) ||
        NULL == body) {
        return -1;
    }

    return http_decoder_half_data_get_raw_body(msg->data, body);
}

int http_message_get_response_raw_body(struct http_message *msg,
                                       struct hstring *body)
{
    if (NULL == msg || (msg->type != HTTP_MESSAGE_RES_BODY) ||
        NULL == body) {
        return -1;
    }

    return http_decoder_half_data_get_raw_body(msg->data, body);
}

int http_message_get_request_decompress_body(struct http_message *msg,
                                             struct hstring *body)
{
    if (NULL == msg || (msg->type != HTTP_MESSAGE_REQ_BODY) ||
        NULL == body) {
        return -1;
    }

    return http_decoder_half_data_get_decompress_body(msg->data, body);
}

int http_message_get_response_decompress_body(struct http_message *msg,
                                              struct hstring *body)
{
    if (NULL == msg || (msg->type != HTTP_MESSAGE_RES_BODY) ||
        NULL == body) {
        return -1;
    }

    return http_decoder_half_data_get_decompress_body(msg->data, body);
}