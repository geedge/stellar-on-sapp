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

#include "stellar/utils.h"
#include "stellar/session.h"
#include "stellar/session_mq.h"
#include "stellar/session_exdata.h"
#include "http_decoder.h"
#include "http_decoder_half.h"
#include "http_decoder_table.h"
#include "llhttp.h"

#define HTTP_IDENTIFY_LEN 16
#define HTTP_DECODER_RESULT_QUEUE_SIZE 16

#define HTTP_DECODER_IS_CACHE_LINE 1
#define HTTP_DECODER_IS_CACHE_BODY 1
#define HTTP_DECODER_IS_CACHE_HEADER 1

const char *http_decoder_topic = "HTTP_DECODER_MESSAGE";

struct http_decoder_result {
    struct session *ref_session;
    struct http_decoder_half_data *req_data;
    struct http_decoder_half_data *res_data;
};

struct http_decoder_result_queue {
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

struct http_decoder_context {
    int plugin_id;
	int topic_id;
    int ex_data_idx;
	struct stellar *st;
	struct http_decoder *decoder;
    struct http_decoder_result_queue *ref_queue;
};

struct http_decoder_result *http_decoder_result_new()
{
    struct http_decoder_result *result = CALLOC(struct http_decoder_result, 1);
    assert(result);

    result->req_data = NULL;
    result->res_data = NULL;

    return result;
}

void http_decoder_result_free(struct http_decoder_result *result)
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
static void http_decoder_result_queue_push(struct http_decoder_result_queue *queue, size_t index)
{
    assert(queue);
    assert(index < queue->queue_size);

    if (queue->array[index] == NULL) {
        queue->array[index] = http_decoder_result_new();
        assert(queue->array[index]);
    }
}

// Remove the http result from the queue but not destroy it
static void http_decoder_result_queue_pop(struct http_decoder_result_queue *queue, size_t index)
{
    assert(queue);
    assert(index < queue->queue_size);

    if (queue->array[index] != NULL) {
        http_decoder_result_free(queue->array[index]);
        queue->array[index] = NULL;
    }
}

static void http_decoder_result_queue_inc_req_index(struct http_decoder_result_queue *queue)
{
    assert(queue);

    queue->req_index++;
    queue->req_index = queue->req_index % queue->queue_size;
}

static void http_decoder_result_queue_inc_res_index(struct http_decoder_result_queue *queue)
{
    assert(queue);

    queue->res_index++;
    queue->res_index = queue->res_index % queue->queue_size;
}

static void http_decoder_result_queue_inc_del_index(struct http_decoder_result_queue *queue)
{
    assert(queue);

    queue->del_index++;
    queue->del_index = queue->del_index % queue->queue_size;
}

static void http_decoder_result_queue_gc(struct http_decoder_result_queue *queue, size_t index)
{
    assert(queue);
    assert(index < queue->queue_size);

    if (index == queue->del_index) {
        http_decoder_result_queue_pop(queue, index);
        http_decoder_result_queue_inc_del_index(queue);
    }
}

static void http_event_handler(enum http_event event,
                               struct http_decoder_half_data **data,
                               void *cb_args)
{
    struct http_decoder_context *ctx = (struct http_decoder_context *)cb_args;
    assert(ctx);

    printf("ctx->topic_id:%d\n", ctx->topic_id);
    assert(ctx->ref_queue);
    struct http_decoder_result_queue *queue = ctx->ref_queue;

    switch (event) {
    case HTTP_EVENT_INIT:
        *data = http_decoder_half_data_new();
        break;
    case HTTP_EVENT_REQ_LINE:
        if (NULL == queue->array[queue->req_index]) {
            http_decoder_result_queue_push(queue, queue->req_index);
        }
        queue->array[queue->req_index]->req_data = *data;
        break;
    case HTTP_EVENT_REQ_HDR:
        break;
    case HTTP_EVENT_REQ_HDR_END:
        break;
    case HTTP_EVENT_REQ_BODY_BEGIN:
        break;
    case HTTP_EVENT_REQ_BODY_DATA:
        break;
    case HTTP_EVENT_REQ_BODY_END:
        break;
    case HTTP_EVENT_REQ_END:
        http_decoder_result_queue_inc_req_index(queue);
		http_decoder_result_queue_gc(queue, queue->req_index);
        break;
    case HTTP_EVENT_RES_LINE:
        queue->array[queue->res_index]->res_data = *data;
        break;
    case HTTP_EVENT_RES_HDR:
        break;
    case HTTP_EVENT_RES_HDR_END:
        break;
    case HTTP_EVENT_RES_BODY_BEGIN:
        break;
    case HTTP_EVENT_RES_BODY_DATA:
        break;
    case HTTP_EVENT_RES_BODY_END:
        break;
    case HTTP_EVENT_RES_END:
        break;
    default:
        assert(0);
        break;
    }
}

static struct http_decoder *http_decoder_new()
{
	struct http_decoder *decoder = CALLOC(struct http_decoder, 1);
	assert(decoder);

	decoder->c2s_half = http_decoder_half_new();
	decoder->s2c_half = http_decoder_half_new();

	return decoder;
}

void http_decoder_reinit(struct http_decoder *decoder, void *args)
{
    http_decoder_half_init(decoder->c2s_half, http_event_handler, args,
						   HTTP_DECODER_IS_CACHE_LINE,
        				   HTTP_DECODER_IS_CACHE_BODY,
						   HTTP_DECODER_IS_CACHE_HEADER);

	http_decoder_half_init(decoder->s2c_half, http_event_handler, args,
						   HTTP_DECODER_IS_CACHE_LINE,
        				   HTTP_DECODER_IS_CACHE_BODY,
						   HTTP_DECODER_IS_CACHE_HEADER);
}

static struct http_decoder_result_queue *http_decoder_result_queue_new(size_t queue_size)
{
    struct http_decoder_result_queue *queue = CALLOC(struct http_decoder_result_queue, 1);
    assert(queue);

    queue->del_index = 0;
    queue->req_index = 0;
    queue->res_index = 0;
    queue->queue_size = queue_size;

    queue->array = CALLOC(struct http_decoder_result *, queue->queue_size);
    assert(queue->array);

    return queue;
}

static void http_decoder_result_queue_free(struct http_decoder_result_queue *queue)
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

static struct http_decoder_context *http_decoder_context_new()
{
	struct http_decoder_context *ctx = CALLOC(struct http_decoder_context, 1);
	assert(ctx);

	return ctx;
}

static void http_decoder_context_free(struct http_decoder_context *ctx)
{
	if (NULL == ctx) {
		return;
	}
}

static int http_protocol_identify(const char *data, size_t data_len)
{
	enum llhttp_type type = HTTP_BOTH;
	llhttp_t parser;
	llhttp_settings_t settings;
	enum llhttp_errno error;

	llhttp_settings_init(&settings);
	llhttp_init(&parser, type, &settings);

	error = llhttp_execute(&parser, data, data_len);
	if (error != HPE_OK) {
		return -1;
	}

	return 0;
}

int http_decoder_entry(struct session *s, int events, const struct packet *pkt, void *cb_arg)
{
	int ret = 0;
	size_t payload_len = 0;
	struct http_decoder_context *ctx = (struct http_decoder_context *)cb_arg;

	const char *payload = session_get0_current_payload(s, &payload_len);
	size_t http_identify_len = payload_len > HTTP_IDENTIFY_LEN ? HTTP_IDENTIFY_LEN : payload_len;

	struct http_decoder_result_queue *queue = session_get_ex_data(s, ctx->ex_data_idx);
	if (NULL == queue) {
		ret = http_protocol_identify(payload, http_identify_len);
		printf("http_protocol_identify ret:%d\n", ret);
		if (ret < 0) {
			//ignore this session's event
			struct session_event *s_event = session_get_intrinsic_event(s, ctx->plugin_id);
			session_event_assign(s_event, ctx->st, s, 0, http_decoder_entry, ctx);
			return 0;
		}

		queue = http_decoder_result_queue_new(HTTP_DECODER_RESULT_QUEUE_SIZE);
		session_set_ex_data(s, ctx->ex_data_idx, queue);
	}

    ctx->ref_queue = queue;
	
    int dir = packet_get_direction(pkt);
    if (dir < 0) {
        return -1;
    }

    // printf("\n-------------------------------------------\n");
    // for (size_t i = 0; i < payload_len; i++) {
    //     printf(" %x", payload[i]);
    // }
    // printf("\n-------------------------------------------\n");

    if (NULL == ctx->decoder) {
        ctx->decoder = http_decoder_new();
        http_decoder_reinit(ctx->decoder, ctx);
    }

    struct http_decoder_half *cur_half = NULL;
	if (dir == PACKET_DIRECTION_C2S) {
		cur_half = ctx->decoder->c2s_half;
	} else {
		cur_half = ctx->decoder->s2c_half;
	}

	ret = http_decoder_half_parse(cur_half, payload, payload_len);
	if (ret < 0) {
		if (dir == PACKET_DIRECTION_C2S) {
            http_decoder_result_queue_pop(queue, queue->req_index);
        } else {
            http_decoder_result_queue_pop(queue, queue->res_index);
        }
    }

	// struct http_message *msg = CALLOC(struct http_message, 1);
	// msg->type = HTTP_MESSAGE_REQ_LINE;
	// session_mq_publish_message(s, ctx->topic_id, msg);

    return 0;
}

static void http_decoder_ex_data_free(struct session *s, int idx, void *ex_ptr, void *arg)
{
	if (ex_ptr != NULL) {
		FREE(ex_ptr);
	}
}

void http_message_free(void *msg, void *cb_arg)
{
	if (NULL == msg) {
		return;
	}

	struct http_message *message = (struct http_message *)msg;
	message->data = NULL; //don't have memory's ownership

	FREE(message);
}

void *http_decoder_init(struct stellar *st)
{
    struct http_decoder_context *ctx = http_decoder_context_new();

    ctx->st = st;
    ctx->ex_data_idx = stellar_session_get_ex_new_index(st, "HTTP_DECODER",
														http_decoder_ex_data_free, NULL);

    int plugin_id = stellar_plugin_register(st, SESS_EV_TCP, http_decoder_entry, ctx);
    if (plugin_id >= 0) {
        ctx->plugin_id = plugin_id;
    }

    int topic_id = session_mq_get_topic_id(st, http_decoder_topic);
    if (topic_id < 0) {
        topic_id = session_mq_create_topic(st, http_decoder_topic, http_message_free, NULL);
    }
    ctx->topic_id = topic_id;

    printf("http_decoder_init: ex_data_idx:%d, plugin_id:%d, topic_id:%d\n",
           ctx->ex_data_idx, ctx->plugin_id, ctx->topic_id);
    return ctx;
}

void http_decoder_exit(void *ctx)
{
	if (NULL == ctx) {
        return;
    }

    FREE(ctx);
}

enum http_message_type http_message_type(struct http_message *msg)
{
	if (NULL == msg) {
		return HTTP_MESSAGE_MAX;
	}

	return msg->type;
}

int http_message_get_request_line(struct http_message *msg, struct http_request_line *line)
{
	if (NULL == msg || msg->type != HTTP_MESSAGE_REQ_LINE ||
	    NULL == line) {
		return -1;
	}

	return http_decoder_half_data_get_request_line(msg->data, line);
}

int http_message_get_response_line(struct http_message *msg, struct http_response_line *line)
{
	if (NULL == msg || msg->type != HTTP_MESSAGE_RES_LINE ||
		NULL == line) {
		return -1;
	}

	return http_decoder_half_data_get_response_line(msg->data, line);
}

int http_message_get_request_header(struct http_message *msg, struct hstring *key,
									struct http_header *header_array, size_t array_size)
{
	if (NULL == msg ||
		(msg->type != HTTP_MESSAGE_REQ_HEADER && msg->type != HTTP_MESSAGE_REQ_HEADER_END) ||
		NULL == header_array || 0 == array_size) {
		return -1;
	}

	return http_decoder_half_data_get_header(msg->data, key, header_array, array_size);
}

int http_message_get_response_header(struct http_message *msg, struct hstring *key,
									 struct http_header *header_array, size_t array_size)
{
	if (NULL == msg ||
		(msg->type != HTTP_MESSAGE_RES_HEADER && msg->type != HTTP_MESSAGE_RES_HEADER_END) ||
		NULL == header_array || 0 == array_size) {
		return -1;
	}

	return http_decoder_half_data_get_header(msg->data, key, header_array, array_size);
}

int http_message_request_header_next(struct http_message *msg, struct http_header *header)
{
	return 0;
}

int http_message_response_header_next(struct http_message *msg, struct http_header *header)
{
	return 0;
}

void http_message_get_request_raw_body(struct http_message *msg, struct hstring *body)
{

}

void http_message_get_response_raw_body(struct http_message *msg, struct hstring *body)
{

}

void http_message_get_request_decompress_body(struct http_message *msg, struct hstring *body)
{

}

void http_message_get_response_decompress_body(struct http_message *msg, struct hstring *body)
{

}