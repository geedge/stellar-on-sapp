/*
**********************************************************************************************
*	File: http_decoder_half.h
*   Description: 
*	Authors: LuWenPeng <luwenpeng@geedgenetworks.com>
*	Date:    2022-10-31
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

#ifndef _HTTP_DECODER_HALF_H_
#define _HTTP_DECODER_HALF_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

#include "http_decoder.h"
#include "http_content_decompress.h"

// only one http event is fired at a time
enum http_event {
    HTTP_EVENT_INIT = 0,

    HTTP_EVENT_REQ_INIT = 1 << 1,
    HTTP_EVENT_REQ_LINE = 1 << 2,
    HTTP_EVENT_REQ_HDR = 1 << 3,
    HTTP_EVENT_REQ_HDR_END = 1 << 4,
    HTTP_EVENT_REQ_BODY_BEGIN = 1 << 5,
    HTTP_EVENT_REQ_BODY_DATA = 1 << 6,
    HTTP_EVENT_REQ_BODY_END = 1 << 7,
    HTTP_EVENT_REQ_END = 1 << 8,

    HTTP_EVENT_RES_INIT = 1 << 9,
    HTTP_EVENT_RES_LINE = 1 << 10,
    HTTP_EVENT_RES_HDR = 1 << 11,
    HTTP_EVENT_RES_HDR_END = 1 << 12,
    HTTP_EVENT_RES_BODY_BEGIN = 1 << 13,
    HTTP_EVENT_RES_BODY_DATA = 1 << 14,
    HTTP_EVENT_RES_BODY_END = 1 << 15,
    HTTP_EVENT_RES_END = 1 << 16,
};

struct http_decoder_half;
struct http_decoder_half_data;

typedef void http_event_cb(enum http_event event, struct http_decoder_half_data **data, void *cb_args);

struct http_decoder_half *
http_decoder_half_new(http_event_cb *event_cb, int is_cache_line,
                      int is_cache_header, int is_cache_body);

void http_decoder_half_free(struct http_decoder_half *half);

int http_decoder_half_parse(struct http_decoder_half *half, void *ev_ctx,
                            const char *data, size_t len);

//http decoder half data API
struct http_decoder_half_data *http_decoder_half_data_new();

void http_decoder_half_data_free(struct http_decoder_half_data *data);

int http_decoder_half_data_get_request_line(struct http_decoder_half_data *data,
                                            struct http_request_line *line);

int http_decoder_half_data_get_response_line(struct http_decoder_half_data *data,
                                             struct http_response_line *line);

int http_decoder_half_data_get_header(struct http_decoder_half_data *data,
                                      struct hstring *key,
                                      struct http_header *header_array,
                                      size_t array_size);

int http_decoder_half_data_iter_header(struct http_decoder_half_data *data,
                                       struct http_header *header);

int http_decoder_half_data_get_raw_body(struct http_decoder_half_data *data,
                                        struct hstring *body);

int http_decoder_half_data_get_decompress_body(struct http_decoder_half_data *data,
                                                       struct hstring *body);

#ifdef __cplusplus
}
#endif

#endif