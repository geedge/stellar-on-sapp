/*
**********************************************************************************************
*	File: http_decoder_half.c
*   Description: 
*	Authors: LuWenPeng <luwenpeng@geedgenetworks.com>
*	Date:    2022-10-31
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

#include <assert.h>
#include <stdio.h>

#include "stellar/utils.h"
#include "llhttp.h"
#include "http_decoder_utils.h"
#include "http_decoder_half.h"
#include "http_decoder_table.h"
#include "http_content_decompress.h"

struct http_decoder_half_data {
    struct http_decoder_table *table;

    int major_version;
    int minor_version;
    int status_code;

    enum http_content_encoding content_encoding;
    char *decompress_body;
    size_t decompress_body_len;
};

struct http_decoder_half {
    llhttp_t parser;
    llhttp_settings_t settings;
    enum llhttp_errno error;

    int is_cache_line;
    int is_cache_header;
    int is_cache_body;

    struct http_decoder_half_data *data;
    enum http_event event;
    http_event_cb *event_cb;
    void *cb_args;
};

struct http_decoder_half *http_decoder_half_new()
{
    struct http_decoder_half *half = CALLOC(struct http_decoder_half, 1);
    assert(half);

    return half;
}

void http_decoder_half_free(struct http_decoder_half *half)
{
    if (NULL == half) {
        return;
    }

    if (half->data != NULL) {
        http_decoder_half_data_free(half->data);
        half->data = NULL;
    }
   
    FREE(half);
}

#ifdef HTTP_DECODER_DEBUG
static void printf_debug_info(const char *desc, const char *at, size_t length)
{
    if (at)
    {
        char *temp = safe_dup(at, length);
        printf("HTTP PARSER STAGE: %s: %s\n", desc, temp);
        safe_free(temp);
    }
    else
    {
        printf("HTTP PARSER STAGE: %s\n", desc);
    }
}
#else
#define printf_debug_info(desc, at, length)
#endif

static void http_decoder_half_data_decompress(struct http_decoder_half_data *data)
{
    assert(data);

    FREE(data->decompress_body);
    data->decompress_body_len = 0;

    if (data->content_encoding == HTTP_CONTENT_ENCODING_NONE) {
        return;
    }

    struct hstring raw_body = {0};
    http_decoder_table_get_body(data->table, &raw_body);
    if (raw_body.str == NULL || raw_body.str_len == 0) {
        return;
    }

    struct http_content_decompress *decompress = http_content_decompress_create(data->content_encoding);
    assert(decompress);
    if (http_content_decompress_write(decompress, raw_body.str, raw_body.str_len,
                                      &data->decompress_body,
                                      &data->decompress_body_len) == -1) {
        // log error
    }
    http_content_decompress_destroy(decompress);
}

/* Possible return values 0, -1, `HPE_PAUSED` */
static int on_message_begin(llhttp_t *http)
{
    printf_debug_info("on_message_begin", NULL, 0);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);
    assert(half->data == NULL);

    half->event = HTTP_EVENT_INIT;
    if (half->event_cb != NULL) {
        half->event_cb(half->event, &half->data, half->cb_args);
    }

    return 0;
}

static int on_message_complete(llhttp_t *http)
{
    printf_debug_info("on_message_complete", NULL, 0);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    //trigger http_body end
    if (half->parser.type == HTTP_REQUEST) {
        if (half->event == HTTP_EVENT_REQ_BODY_DATA) {
            half->event = HTTP_EVENT_REQ_BODY_END;
            if (half->event_cb != NULL) {
                half->event_cb(half->event, &half->data, half->cb_args);
            }
        }
    } else {
        if (half->event == HTTP_EVENT_RES_BODY_DATA) {
            half->event = HTTP_EVENT_RES_BODY_END;
            if (half->event_cb != NULL) {
                half->event_cb(half->event, &half->data, half->cb_args);
            }
        }
    }

    //trigger req/res end
    if (half->parser.type == HTTP_REQUEST) {
        half->event = HTTP_EVENT_REQ_END;
        if (half->event_cb != NULL) {
            half->event_cb(half->event, &half->data, half->cb_args);
        }
    } else {
        half->event = HTTP_EVENT_RES_END;
        if (half->event_cb != NULL) {
            half->event_cb(half->event, &half->data, half->cb_args);
        }
    }

    return 0;
}

static int on_reset(llhttp_t *http)
{
    printf_debug_info("on_reset", NULL, 0);

    return 0;
}

static int on_method(llhttp_t *http, const char *at, size_t length)
{
    printf_debug_info("on_method", at, length);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->data != NULL) {
        http_decoder_table_refer(half->data->table, HTTP_ITEM_METHOD, at, length);
    }

    return 0;
}

/* Information-only callbacks, return value is ignored */
static int on_method_complete(llhttp_t *http)
{
    printf_debug_info("on_method_complete", NULL, 0);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->data != NULL) {
        // if (http_decoder_table_state(half->data->table, HTTP_ITEM_METHOD) == STRING_STATE_REFER) {
        //     http_decoder_table_cache(half->data->table, HTTP_ITEM_METHOD);
        // }
        http_decoder_table_commit(half->data->table, HTTP_ITEM_METHOD);
    }

    return 0;
}

/* Possible return values 0, -1, HPE_USER */
static int on_uri(llhttp_t *http, const char *at, size_t length)
{
    printf_debug_info("on_uri", at, length);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->data != NULL) {
        http_decoder_table_refer(half->data->table, HTTP_ITEM_URI, at, length);
    }

    return 0;
}

/* Information-only callbacks, return value is ignored */
static int on_uri_complete(llhttp_t *http)
{
    printf_debug_info("on_uri_complete", NULL, 0);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->data != NULL) {
        // if (half->is_cache_line && http_decoder_table_state(half->data->table, HTTP_ITEM_URI) == STRING_STATE_REFER)
        // {
        //     http_decoder_table_cache(half->data->table, HTTP_ITEM_URI);
        // }
        http_decoder_table_commit(half->data->table, HTTP_ITEM_URI);
    }

    return 0;
}

/* Possible return values 0, -1, HPE_USER */
static int on_version(llhttp_t *http, const char *at, size_t length)
{
    printf_debug_info("on_version", at, length);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->data != NULL) {
        http_decoder_table_refer(half->data->table, HTTP_ITEM_VERSION, at, length);
    }

    return 0;
}

/* Information-only callbacks, return value is ignored */
static int on_version_complete(llhttp_t *http)
{
     printf_debug_info("on_version_complete", NULL, 0);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->data) {
        // if (half->is_cache_line && http_decoder_table_state(half->data->table, HTTP_ITERM_VERSION) == STRING_STATE_REFER)
        // {
        //     http_decoder_table_cache(half->data->table, HTTP_ITERM_VERSION);
        // }
        http_decoder_table_commit(half->data->table, HTTP_ITEM_VERSION);

        half->data->major_version = llhttp_get_http_major(&half->parser);
        half->data->minor_version = llhttp_get_http_minor(&half->parser);
    }

    if (half->parser.type == HTTP_REQUEST) {
        half->event = (enum http_event)HTTP_EVENT_REQ_LINE;
        if (half->event_cb) {
            half->event_cb(half->event, &half->data, half->cb_args);
        }
    }

    return 0;
}

/* Possible return values 0, -1, HPE_USER */
static int on_status(llhttp_t *http, const char *at, size_t length)
{
    printf_debug_info("on_status", at, length);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->data != NULL) {
        http_decoder_table_refer(half->data->table, HTTP_ITEM_STATUS, at, length);
    }

    return 0;
}

/* Information-only callbacks, return value is ignored */
static int on_status_complete(llhttp_t *http)
{
    printf_debug_info("on_status_complete", NULL, 0);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->data != NULL) {
        // if (half->is_cache_line && http_decoder_table_state(half->data->table, HTTP_ITERM_STATUS) == STRING_STATE_REFER)
        // {
        //     http_decoder_table_cache(half->data->table, HTTP_ITERM_STATUS);
        // }
        http_decoder_table_commit(half->data->table, HTTP_ITEM_STATUS);
        half->data->status_code = llhttp_get_status_code(&half->parser);
    }

    half->event = (enum http_event)HTTP_EVENT_RES_LINE;
    if (half->event_cb != NULL) {
        half->event_cb(half->event, &half->data, half->cb_args);
    }

    return 0;
}

/* Possible return values 0, -1, HPE_USER */
static int on_header_field(llhttp_t *http, const char *at, size_t length)
{
    printf_debug_info("on_header_field", at, length);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->data != NULL) {
        http_decoder_table_refer(half->data->table, HTTP_ITEM_HDRKEY, at, length);
    }

    return 0;
}

/* Information-only callbacks, return value is ignored */
static int on_header_field_complete(llhttp_t *http)
{
    printf_debug_info("on_header_field_complete", NULL, 0);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->data != NULL) {
        // if (half->is_cache_header && http_decoder_table_state(half->data->table, HTTP_ITERM_HDRKEY) == STRING_STATE_REFER)
        // {
        //     http_decoder_table_cache(half->data->table, HTTP_ITERM_HDRKEY);
        // }
        http_decoder_table_commit(half->data->table, HTTP_ITEM_HDRKEY);
    }

    return 0;
}

/* Possible return values 0, -1, HPE_USER */
static int on_header_value(llhttp_t *http, const char *at, size_t length)
{
    printf_debug_info("on_header_value", at, length);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->data != NULL) {
        http_decoder_table_refer(half->data->table, HTTP_ITEM_HDRVAL, at, length);
    }

    return 0;
}

/* Information-only callbacks, return value is ignored */
static int on_header_value_complete(llhttp_t *http)
{
    printf_debug_info("on_header_value_complete", NULL, 0);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->data != NULL) {
        // if (half->is_cache_header &&
        //     http_decoder_table_state(half->data->table, HTTP_ITERM_HDRVAL) ==
        //         STRING_STATE_REFER) {
        //     http_decoder_table_cache(half->data->table, HTTP_ITERM_HDRVAL);
        // }
        http_decoder_table_commit(half->data->table, HTTP_ITEM_HDRVAL);
    }

    if (half->parser.type == HTTP_REQUEST) {
        half->event = HTTP_EVENT_REQ_HDR;
        if (half->event_cb) {
            half->event_cb(half->event, &half->data, half->cb_args);
        }
    }

    if (half->parser.type == HTTP_RESPONSE) {
        half->event = HTTP_EVENT_RES_HDR;
        if (half->event_cb) {
            half->event_cb(half->event, &half->data, half->cb_args);
        }
    }

    if (half->data != NULL) {
        if (half->data->content_encoding == HTTP_CONTENT_ENCODING_NONE) {
            struct http_header http_hdr = {0};
            struct hstring key = {.str = (char *)"Content-Encoding", .str_len = 16};
            if (http_decoder_table_get_header(half->data->table, &key, &http_hdr, 1) == 1) {
                char *str = safe_dup(http_hdr.val.str, http_hdr.val.str_len);
                half->data->content_encoding = http_content_encoding_str2int(str);
                FREE(str);
            }
        }
    }

    return 0;
}

/* When on_chunk_header is called, the current chunk length is stored
 * in parser->content_length.
 * Possible return values 0, -1, `HPE_PAUSED`
 */
static int on_chunk_header(llhttp_t *http)
{
    printf_debug_info("on_chunk_header", NULL, 0);

    return 0;
}

/* When on_chunk_header is called, the current chunk length is stored
 * in parser->content_length.
 * Possible return values 0, -1, `HPE_PAUSED`
 */
static int on_chunk_header_complete(llhttp_t *http)
{
    printf_debug_info("on_chunk_header_complete", NULL, 0);

    return 0;
}

/* Possible return values:
 *  0  - Proceed normally
 *  1  - Assume that request/response has no body, and proceed to parsing the next message
 *  2  - Assume absence of body (as above) and make `llhttp_execute()` return `HPE_PAUSED_UPGRADE`
 * -1  - Error `HPE_PAUSED`
 */
static int on_headers_complete(llhttp_t *http)
{
    printf_debug_info("on_headers_complete", NULL, 0);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->parser.type == HTTP_REQUEST) {
        half->event = HTTP_EVENT_REQ_HDR_END;
        if (half->event_cb) {
            half->event_cb(half->event, &half->data, half->cb_args);
        }
    }

    if (half->parser.type == HTTP_RESPONSE) {
        half->event = HTTP_EVENT_RES_HDR_END;
        if (half->event_cb) {
            half->event_cb(half->event, &half->data, half->cb_args);
        }
    }

    return 0;
}

/* Possible return values 0, -1, HPE_USER */
static int on_body(llhttp_t *http, const char *at, size_t length)
{
    printf_debug_info("on_body", at, length);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    // trigger body_begin event
    if (half->parser.type == HTTP_REQUEST) {
        if (half->event == HTTP_EVENT_REQ_HDR_END) {
            half->event = HTTP_EVENT_REQ_BODY_BEGIN;
            if (half->event_cb) {
                half->event_cb(half->event, &half->data, half->cb_args);
            }
        }
    } else {
        if (half->event == HTTP_EVENT_RES_HDR_END) {
            half->event = HTTP_EVENT_RES_BODY_BEGIN;
            if (half->event_cb) {
                half->event_cb(half->event, &half->data, half->cb_args);
            }
        }
    }

    // if enable  cache_body, trigger body_data event on_message_complete
    // if disable cache_body, trigger body_data event on_body
    // if (half->is_cache_body) {
    //     if (half->data) {
    //         http_decoder_table_refer(half->data->table, HTTP_ITERM_BODY, at,
    //                                  length);
    //         http_decoder_table_cache(half->data->table, HTTP_ITERM_BODY);
    //     }
    // } else {
        if (half->data != NULL) {
            if (http_decoder_table_state(half->data->table, HTTP_ITEM_BODY) == STRING_STATE_COMMIT) {
                http_decoder_table_reset(half->data->table, HTTP_ITEM_BODY);
            }

            http_decoder_table_refer(half->data->table, HTTP_ITEM_BODY, at, length);
            http_decoder_table_commit(half->data->table, HTTP_ITEM_BODY);

            http_decoder_half_data_decompress(half->data);
        }

        if (half->parser.type == HTTP_REQUEST) {
            half->event = HTTP_EVENT_REQ_BODY_DATA;
            if (half->event_cb) {
                half->event_cb(half->event, &half->data, half->cb_args);
            }
        } else {
            half->event = HTTP_EVENT_RES_BODY_DATA;
            if (half->event_cb) {
                half->event_cb(half->event, &half->data, half->cb_args);
            }
        }
    // }

    return 0;
}

void http_decoder_half_init(struct http_decoder_half *half, http_event_cb *event_cb, 
                            void *cb_args, int is_cache_line, int is_cache_header,
                            int is_cache_body)
{
    if (NULL == half) {
        return;
    }

    enum llhttp_type type = HTTP_BOTH; // HTTP_BOTH | HTTP_REQUEST | HTTP_RESPONSE
    llhttp_settings_init(&half->settings);
    llhttp_init(&half->parser, type, &half->settings);

    half->settings.on_message_begin = on_message_begin;
    half->settings.on_message_complete = on_message_complete;
    half->settings.on_reset = on_reset;

    half->settings.on_url = on_uri;
    half->settings.on_url_complete = on_uri_complete;

    half->settings.on_status = on_status;
    half->settings.on_status_complete = on_status_complete;

    half->settings.on_method = on_method;
    half->settings.on_method_complete = on_method_complete;

    half->settings.on_version = on_version;
    half->settings.on_version_complete = on_version_complete;

    half->settings.on_header_field = on_header_field;
    half->settings.on_header_field_complete = on_header_field_complete;

    half->settings.on_header_value = on_header_value;
    half->settings.on_header_value_complete = on_header_value_complete;

    half->settings.on_chunk_header = on_chunk_header;
    half->settings.on_chunk_complete = on_chunk_header_complete;

    half->settings.on_headers_complete = on_headers_complete;
    half->settings.on_body = on_body;

    half->error = HPE_OK;

    half->event_cb = event_cb;
    half->cb_args = cb_args; //result queue pointer
    half->is_cache_line = is_cache_line;
    half->is_cache_header = is_cache_header;
    half->is_cache_body = is_cache_body;

    half->data = NULL;
}

int http_decoder_half_parse(struct http_decoder_half *half, const char *data,
                            size_t data_len)
{
    if (NULL == half || NULL == data || 0 == data_len) {
        return -1;
    }

    half->error = llhttp_execute(&half->parser, data, data_len);
    if (half->error != HPE_OK) {
        printf("llhttp_execute parse error: %s %s\n",
                llhttp_errno_name(half->error), half->parser.reason);
        return -1;
    }

    if (half->data != NULL) {
        if (http_decoder_table_state(half->data->table, HTTP_ITEM_URI) ==
            STRING_STATE_REFER) {
            http_decoder_table_cache(half->data->table, HTTP_ITEM_URI);
        }

        if (http_decoder_table_state(half->data->table, HTTP_ITEM_STATUS) ==
            STRING_STATE_REFER) {
            http_decoder_table_cache(half->data->table, HTTP_ITEM_STATUS);
        }

        if (http_decoder_table_state(half->data->table, HTTP_ITEM_METHOD) ==
            STRING_STATE_REFER) {
            http_decoder_table_cache(half->data->table, HTTP_ITEM_METHOD);
        }

        if (http_decoder_table_state(half->data->table, HTTP_ITEM_VERSION) ==
            STRING_STATE_REFER) {
            http_decoder_table_cache(half->data->table, HTTP_ITEM_VERSION);
        }

        if (http_decoder_table_state(half->data->table, HTTP_ITEM_HDRKEY) ==
            STRING_STATE_REFER) {
            http_decoder_table_cache(half->data->table, HTTP_ITEM_HDRKEY);
        }

        if (http_decoder_table_state(half->data->table, HTTP_ITEM_HDRVAL) ==
            STRING_STATE_REFER) {
            http_decoder_table_cache(half->data->table, HTTP_ITEM_HDRVAL);
        }

        if (http_decoder_table_state(half->data->table, HTTP_ITEM_BODY) ==
            STRING_STATE_REFER) {
            http_decoder_table_cache(half->data->table, HTTP_ITEM_BODY);
        }
    }

    return 0;
}

struct http_decoder_half_data *http_decoder_half_data_new()
{
    struct http_decoder_half_data *data = CALLOC(struct http_decoder_half_data, 1);
    assert(data);

    data->table = http_decoder_table_new();
    assert(data->table);

    data->major_version = -1;
    data->minor_version = -1;
    data->status_code = -1;

    data->content_encoding = HTTP_CONTENT_ENCODING_NONE;
    data->decompress_body = NULL;
    data->decompress_body_len = 0;

    return data;
}

void http_decoder_half_data_free(struct http_decoder_half_data *data)
{
    if (NULL == data) {
        return;
    }

    if (data->table != NULL) {
        http_decoder_table_free(data->table);
        data->table = NULL;
    }

    if (data->decompress_body != NULL) {
        FREE(data->decompress_body);
    }

    FREE(data);
}

int http_decoder_half_data_get_request_line(struct http_decoder_half_data *data,
                                            struct http_request_line *line)
{
    if (NULL == data || NULL == line) {
        return -1;
    }

    http_decoder_table_get_method(data->table, &line->method);
    http_decoder_table_get_uri(data->table, &line->uri);
    http_decoder_table_get_version(data->table, &line->version);

    line->major_version = data->major_version;
    line->minor_version = data->minor_version;

    return 0;
}

int http_decoder_half_data_get_response_line(struct http_decoder_half_data *data,
                                             struct http_response_line *line)
{
    if (NULL == data || NULL == line) {
        return -1;
    }

    http_decoder_table_get_version(data->table, &line->version);
    http_decoder_table_get_status(data->table, &line->status);

    line->major_version = data->major_version;
    line->minor_version = data->minor_version;
    line->status_code = data->status_code;

    return 0;
}

int http_decoder_half_data_get_header(struct http_decoder_half_data *data,
                                      struct hstring *key,
                                      struct http_header *header_array,
                                      size_t array_size)
{
    http_decoder_table_get_header(data->table, key, header_array, array_size);
}

int http_decoder_half_data_iter_header(struct http_decoder_half_data *data,
                                       struct http_header *header)
{
    if (NULL == data || NULL == data->table) {
        return -1;
    }

    http_decoder_table_iter_header(data->table, header);
}