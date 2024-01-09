/*
**********************************************************************************************
*	File: http_decoder_entry.c
*   Description: 
*	Authors: LuWenPeng <luwenpeng@geedgenetworks.com>
*	Date:    2022-10-31
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
#include "llhttp.h"
#include "fieldstat/fieldstat_easy.h"

#define HTTP_IDENTIFY_LEN 16
#define HD_RESULT_QUEUE_SIZE 16
#define HD_IS_CACHE_BODY 1

const char *http_decoder_topic = "HTTP_DECODER_MESSAGE";
const char *fs_file_name = "http_decoder.fs";

struct http_decoder_result {
    struct http_decoder_half_data *req_data;
    struct http_decoder_half_data *res_data;
};

struct http_decoder_result_queue {
    struct session *ref_session;
	size_t req_index;
	size_t res_index;
	size_t del_index;
	size_t queue_size;
	struct http_decoder_result **array;
};

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

struct http_event_context {
    int topic_id;
    struct session *ref_session;
    struct http_decoder_result_queue *ref_queue;
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
    struct http_decoder *decoder;
    struct http_event_context *http_ev_ctx;
};

static struct http_decoder_result *
http_decoder_result_new()
{
    struct http_decoder_result *result =
        CALLOC(struct http_decoder_result, 1);
    assert(result);

    result->req_data = NULL;
    result->res_data = NULL;

    return result;
}

static void
http_decoder_result_free(struct http_decoder_result *result)
{
	if (NULL == result) {
		return;
	}

	if (result->req_data != NULL) {
		http_decoder_half_data_free(result->req_data);
		result->req_data = NULL;
	}
    
	if (result->res_data != NULL) {
		http_decoder_half_data_free(result->res_data);
		result->res_data = NULL;
	}

    FREE(result);
}

// Create a new http result and add it to the queue
static void
http_decoder_result_queue_push(struct http_decoder_result_queue *queue,
    size_t index)
{
    assert(queue);
    assert(index < queue->queue_size);

    if (queue->array[index] == NULL) {
        queue->array[index] = http_decoder_result_new();
        assert(queue->array[index]);
    }
}

// Remove the http result from the queue but not destroy it
static void
http_decoder_result_queue_pop(struct http_decoder_result_queue *queue,
    size_t index)
{
    assert(queue);
    assert(index < queue->queue_size);

    if (queue->array[index] != NULL) {
        http_decoder_result_free(queue->array[index]);
        queue->array[index] = NULL;
    }
}

static void
http_decoder_result_queue_inc_req_index(struct http_decoder_result_queue *queue)
{
    assert(queue);

    queue->req_index++;
    queue->req_index = queue->req_index % queue->queue_size;
}

static void
http_decoder_result_queue_inc_res_index(struct http_decoder_result_queue *queue)
{
    assert(queue);

    queue->res_index++;
    queue->res_index = queue->res_index % queue->queue_size;
}

static void
http_decoder_result_queue_inc_del_index(struct http_decoder_result_queue *queue)
{
    assert(queue);

    queue->del_index++;
    queue->del_index = queue->del_index % queue->queue_size;
}

static void
http_decoder_result_queue_gc(struct http_decoder_result_queue *queue,
    size_t index)
{
    assert(queue);
    assert(index < queue->queue_size);

    if (index == queue->del_index) {
        http_decoder_result_queue_pop(queue, index);
        http_decoder_result_queue_inc_del_index(queue);
    }
}

static void
http_event_handler(enum http_event event, struct http_decoder_half_data **data,
    void *http_ev_ctx)
{
    struct http_event_context *ctx = (struct http_event_context *)http_ev_ctx;
    assert(ctx);

    struct http_decoder_result_queue *queue = ctx->ref_queue;
    struct http_message *msg = NULL;

    switch (event) {
    case HTTP_EVENT_REQ_INIT:
        queue->array[queue->req_index]->req_data = http_decoder_half_data_new();
        *data = queue->array[queue->req_index]->req_data;
        break;
    case HTTP_EVENT_REQ_LINE:
        msg = CALLOC(struct http_message, 1);
	    msg->type = HTTP_MESSAGE_REQ_LINE;
        msg->data = *data;
        session_mq_publish_message(ctx->ref_session, ctx->topic_id, msg);
        break;
    case HTTP_EVENT_REQ_HDR:
        break;
    case HTTP_EVENT_REQ_HDR_END:
        msg = CALLOC(struct http_message, 1);
	    msg->type = HTTP_MESSAGE_REQ_HEADER;
        msg->data = *data;
        session_mq_publish_message(ctx->ref_session, ctx->topic_id, msg);
        break;
    case HTTP_EVENT_REQ_BODY_BEGIN:
        break;
    case HTTP_EVENT_REQ_BODY_DATA:
        msg = CALLOC(struct http_message, 1);
	    msg->type = HTTP_MESSAGE_REQ_BODY;
        msg->data = *data;
        session_mq_publish_message(ctx->ref_session, ctx->topic_id, msg);
        break;
    case HTTP_EVENT_REQ_BODY_END:
        break;
    case HTTP_EVENT_REQ_END:
        http_decoder_result_queue_inc_req_index(queue);
		http_decoder_result_queue_gc(queue, queue->req_index);
        break;
    case HTTP_EVENT_RES_INIT:
        queue->array[queue->res_index]->res_data = http_decoder_half_data_new();
        *data = queue->array[queue->res_index]->res_data;
        break;
    case HTTP_EVENT_RES_LINE:
        msg = CALLOC(struct http_message, 1);
	    msg->type = HTTP_MESSAGE_RES_LINE;
        msg->data = *data;
        session_mq_publish_message(ctx->ref_session, ctx->topic_id, msg);
        break;
    case HTTP_EVENT_RES_HDR:
        break;
    case HTTP_EVENT_RES_HDR_END:
        msg = CALLOC(struct http_message, 1);
	    msg->type = HTTP_MESSAGE_RES_HEADER;
        msg->data = *data;
        session_mq_publish_message(ctx->ref_session, ctx->topic_id, msg); 
        break;
    case HTTP_EVENT_RES_BODY_BEGIN:
        break;
    case HTTP_EVENT_RES_BODY_DATA:
        msg = CALLOC(struct http_message, 1);
	    msg->type = HTTP_MESSAGE_RES_BODY;
        msg->data = *data;
        session_mq_publish_message(ctx->ref_session, ctx->topic_id, msg); 
        break;
    case HTTP_EVENT_RES_BODY_END:
        break;
    case HTTP_EVENT_RES_END:
        http_decoder_result_queue_inc_res_index(queue);
		http_decoder_result_queue_gc(queue, queue->res_index);
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

	decoder->c2s_half = http_decoder_half_new(ev_cb, is_cache_body);
	decoder->s2c_half = http_decoder_half_new(ev_cb, is_cache_body);

	return decoder;
}

static void
http_decoder_free(struct http_decoder *decoder)
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

static struct http_decoder_result_queue *
http_decoder_result_queue_new(size_t queue_size)
{
    struct http_decoder_result_queue *queue =
        CALLOC(struct http_decoder_result_queue, 1);
    assert(queue);

    queue->del_index = 0;
    queue->req_index = 0;
    queue->res_index = 0;
    queue->queue_size = queue_size;

    queue->array = CALLOC(struct http_decoder_result *, queue->queue_size);
    assert(queue->array);

    for (size_t i = 0; i < queue->queue_size; i++) {
        queue->array[i] = CALLOC(struct http_decoder_result, 1);
    }
    
    return queue;
}

static void
http_decoder_result_queue_free(struct http_decoder_result_queue *queue)
{
	if (NULL == queue) {
		return;
    }

    if (queue->array != NULL) {
        for (size_t i = 0; i < queue->queue_size; i++) {
            if (queue->array[i] != NULL) {
            	http_decoder_result_free(queue->array[i]);
				queue->array[i] = NULL;
            }
        }
        
		FREE(queue->array);
    }

    FREE(queue);
}

static int
http_protocol_identify(const char *data, size_t data_len)
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
    struct http_decoder_context *ctx = (struct http_decoder_context *)cb_arg;
    size_t payload_len = 0;
    uint64_t inner_flag = 0;

    int ret = session_is_inner_most(sess, &inner_flag);
    if (0 == ret) {
        return 0;
    }

    struct http_decoder_result_queue *queue =
        session_get_ex_data(sess, ctx->ex_data_idx);;

    if (events & SESS_EV_CLOSING) {
        if (queue != NULL) {
            http_decoder_result_queue_free(queue);
            session_set_ex_data(sess, ctx->ex_data_idx, NULL); 
        }

        return 0;
    }

    const char *payload = session_get0_current_payload(sess, &payload_len);
    // printf("session:%s\n", session_get0_readable_addr(sess));

    if (events & SESS_EV_OPENING) {
        if (queue != NULL) {
            fprintf(stderr,
                    "http_decoder_result_queue should be null for new session\n");
            return -1;
        }

        //If not http, ignore this session
        if (payload_len > 0) {
            size_t http_identify_len =
                payload_len > HTTP_IDENTIFY_LEN ? HTTP_IDENTIFY_LEN : payload_len;
            ret = http_protocol_identify(payload, http_identify_len);
            if (ret < 0) {
                // ignore this session's event
                struct session_event *s_event =
                    session_get_intrinsic_event(sess, ctx->plugin_id);

                session_event_assign(s_event, ctx->st, sess, 0,
                                     http_decoder_entry, ctx);
                return 0;
            }
        }

        queue = http_decoder_result_queue_new(HD_RESULT_QUEUE_SIZE);
        queue->ref_session = sess;
        session_set_ex_data(sess, ctx->ex_data_idx, queue);
    }

    if (0 == payload_len || NULL == queue) {
        return 0;
    }

    

    int dir = packet_get_direction(pkt);
    if (dir < 0) {
        return -1;
    }

    if (NULL == ctx->decoder) {
        ctx->decoder = http_decoder_new(http_event_handler, 0);
    }

    struct http_decoder_half *cur_half = NULL;
    if (dir == PACKET_DIRECTION_C2S) {
        cur_half = ctx->decoder->c2s_half;
    } else {
        cur_half = ctx->decoder->s2c_half;
    }

    ctx->http_ev_ctx->topic_id = ctx->topic_id;
    ctx->http_ev_ctx->ref_queue = queue;
    ctx->http_ev_ctx->ref_session = sess;

    http_decoder_half_parse(cur_half, ctx->http_ev_ctx, payload, payload_len);

    long long trans_cnt = http_decoder_half_trans_count(cur_half);

    fieldstat_easy_counter_incrby(ctx->fse, 0, ctx->fs_incoming_bytes_id,
                                  NULL, 0, payload_len);
    fieldstat_easy_counter_incrby(ctx->fse, 0, ctx->fs_incoming_pkts_id,
                                  NULL, 0, 1);
    fieldstat_easy_counter_incrby(ctx->fse, 0, ctx->fs_incoming_trans_id,
                                  NULL, 0, trans_cnt);

    return 0;
}

static void
http_decoder_ex_data_free(struct session *s, int idx, void *ex_ptr, void *arg)
{
	if (ex_ptr != NULL) {
		FREE(ex_ptr);
	}
}

static void
http_message_free(void *msg, void *cb_arg)
{
	if (NULL == msg) {
		return;
	}

	struct http_message *message = (struct http_message *)msg;
	message->data = NULL; //don't have memory's ownership

	FREE(message);
}

static void
_http_decoder_context_free(struct http_decoder_context *ctx)
{
    if (NULL == ctx) {
        return;
    }

    if (ctx->http_ev_ctx != NULL) {
        FREE(ctx->http_ev_ctx);
    }

    if (ctx->fse != NULL) {
        fieldstat_easy_free(ctx->fse);
        ctx->fse = NULL;
    }

    if (ctx->topic_id >= 0) {
        session_mq_destroy_topic(ctx->st, ctx->topic_id);
        ctx->topic_id = -1;
    }

    if (ctx->decoder != NULL) {
        http_decoder_free(ctx->decoder);
        ctx->decoder = NULL;
    }

    FREE(ctx);
}

#define FS_OUTPUT_INTERVAL_S 2
void *http_decoder_init(struct stellar *st)
{
    struct http_decoder_context *ctx = CALLOC(struct http_decoder_context, 1);

    ctx->http_ev_ctx = CALLOC(struct http_event_context, 1);
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

    ctx->fse = fieldstat_easy_new(1, "http_decoder_statistics", NULL, 0);
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
    sleep(3);

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

enum http_message_type
http_message_type(struct http_message *msg)
{
	if (NULL == msg) {
		return HTTP_MESSAGE_MAX;
	}

	return msg->type;
}

int
http_message_get_request_line(struct http_message *msg,
    struct http_request_line *line)
{
	if (NULL == msg || msg->type != HTTP_MESSAGE_REQ_LINE || NULL == line) {
		return -1;
	}

	return http_decoder_half_data_get_request_line(msg->data, line);
}

int
http_message_get_response_line(struct http_message *msg,
    struct http_response_line *line)
{
	if (NULL == msg || msg->type != HTTP_MESSAGE_RES_LINE || NULL == line) {
		return -1;
	}

	return http_decoder_half_data_get_response_line(msg->data, line);
}

int
http_message_get_request_header(struct http_message *msg, struct hstring *key,
	struct http_header *header_array, size_t array_size)
{
	if (NULL == msg || msg->type != HTTP_MESSAGE_REQ_HEADER
        || NULL == key || NULL == header_array || 0 == array_size) {
		return -1;
	}

	return http_decoder_half_data_get_header(msg->data, key, header_array,
                                             array_size);
}

int
http_message_get_response_header(struct http_message *msg, struct hstring *key,
	struct http_header *header_array, size_t array_size)
{
	if (NULL == msg || msg->type != HTTP_MESSAGE_RES_HEADER
        || NULL == key || NULL == header_array
        || 0 == array_size) {
		return -1;
	}

	return http_decoder_half_data_get_header(msg->data, key, header_array,
                                             array_size);
}

int
http_message_request_header_next(struct http_message *msg,
    struct http_header *header)
{
    if (NULL == msg || msg->type != HTTP_MESSAGE_REQ_HEADER
        || NULL == header) {
		return -1;
	}

    return http_decoder_half_data_iter_header(msg->data, header);
}

int
http_message_response_header_next(struct http_message *msg,
    struct http_header *header)
{
    if (NULL == msg || msg->type != HTTP_MESSAGE_RES_HEADER
        || NULL == header) {
		return -1;
	}

    return http_decoder_half_data_iter_header(msg->data, header);
}

int
http_message_get_request_raw_body(struct http_message *msg,
    struct hstring *body)
{
    if (NULL == msg || (msg->type != HTTP_MESSAGE_REQ_BODY)
        || NULL == body) {
        return -1;
    }

    return http_decoder_half_data_get_raw_body(msg->data, body);
}

int
http_message_get_response_raw_body(struct http_message *msg,
    struct hstring *body)
{
    if (NULL == msg || (msg->type != HTTP_MESSAGE_RES_BODY)
        || NULL == body) {
        return -1;
    }

    return http_decoder_half_data_get_raw_body(msg->data, body);
}

int
http_message_get_request_decompress_body(struct http_message *msg,
    struct hstring *body)
{
    if (NULL == msg || (msg->type != HTTP_MESSAGE_REQ_BODY)
        || NULL == body) {
        return -1;
    }

    return http_decoder_half_data_get_decompress_body(msg->data, body);
}

int
http_message_get_response_decompress_body(struct http_message *msg,
    struct hstring *body)
{
    if (NULL == msg || (msg->type != HTTP_MESSAGE_RES_BODY)
        || NULL == body) {
        return -1;
    }

    return http_decoder_half_data_get_decompress_body(msg->data, body);
}