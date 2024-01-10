/*
**********************************************************************************************
*	File: http_decoder_table.c
*   Description:
*	Authors: LuWenPeng <luwenpeng@geedgenetworks.com>
*	Date:    2022-10-31
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/


#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "http_decoder_table.h"
#include "http_decoder.h"
#include "http_decoder_string.h"
#include "stellar/utils.h"

#define MAX_HEADER_SIZE 16


struct http_decoder_header {
    struct http_decoder_string key;
    struct http_decoder_string val;
};

struct http_decoder_table {
    struct http_decoder_string uri;
    struct http_decoder_string status;
    struct http_decoder_string method;
    struct http_decoder_string version;
    struct http_decoder_string body;

    size_t header_size;
    size_t header_index;
    size_t header_iter;
    struct http_decoder_header *headers;
};

struct http_decoder_table *
http_decoder_table_new()
{
    struct http_decoder_table *table = CALLOC(struct http_decoder_table, 1);
    assert(table);

    table->header_index = 0;
    table->header_size = MAX_HEADER_SIZE;
    table->headers = CALLOC(struct http_decoder_header, table->header_size);

    return table;
}

void
http_decoder_table_free(struct http_decoder_table *table)
{
    if (NULL == table) {
        return;
    }

    if (table->uri.cache.str != NULL) {
        FREE(table->uri.cache.str);
    }

    if (table->status.cache.str != NULL) {
        FREE(table->status.cache.str);
    }

    if (table->method.cache.str != NULL) {
        FREE(table->method.cache.str);
    }

    if (table->version.cache.str != NULL) {
        FREE(table->version.cache.str);
    }

    if (table->body.cache.str != NULL) {
        FREE(table->body.cache.str);
    }

    if (table->headers != NULL) {
        for (size_t i = 0; i < table->header_size; i++) {
            if (table->headers[i].key.cache.str != NULL) {
                FREE(table->headers[i].key.cache.str);
            }

            if (table->headers[i].val.cache.str != NULL) {
                FREE(table->headers[i].val.cache.str);
            }
        }

        FREE(table->headers);
    }

    FREE(table);
}

enum string_state
http_decoder_table_state(struct http_decoder_table *table, enum http_item type)
{
    if (NULL == table) {
        return STRING_STATE_INIT;
    }

    struct http_decoder_header *header = NULL;
    enum string_state state = STRING_STATE_INIT;
    assert(table);

    switch (type) {
    case HTTP_ITEM_URI:
        state = http_decoder_string_state(&table->uri);
        break;
    case HTTP_ITEM_STATUS:
        state = http_decoder_string_state(&table->status);
        break;
    case HTTP_ITEM_METHOD:
        state = http_decoder_string_state(&table->method);
        break;
    case HTTP_ITEM_VERSION:
        state = http_decoder_string_state(&table->version);
        break;
    case HTTP_ITEM_HDRKEY:
        header = &table->headers[table->header_index];
        state = http_decoder_string_state(&header->key);
        break;
    case HTTP_ITEM_HDRVAL:
        header = &table->headers[table->header_index];
        state = http_decoder_string_state(&header->val);
        break;
    case HTTP_ITEM_BODY:
        state = http_decoder_string_state(&table->body);
        break;
    default:
        abort();
        break;
    }

    return state;
}

void
http_decoder_table_refer(struct http_decoder_table *table, enum http_item type,
    const char *at, size_t len)
{
    if (NULL == table) {
        return;
    }

    struct http_decoder_header *header = NULL;
    assert(table);

    switch (type) {
    case HTTP_ITEM_URI:
        http_decoder_string_refer(&table->uri, at, len);
        break;
    case HTTP_ITEM_STATUS:
        http_decoder_string_refer(&table->status, at, len);
        break;
    case HTTP_ITEM_METHOD:
        http_decoder_string_refer(&table->method, at, len);
        break;
    case HTTP_ITEM_VERSION:
        http_decoder_string_refer(&table->version, at, len);
        break;
    case HTTP_ITEM_HDRKEY:
        if (table->header_index >= table->header_size) {
            table->headers = REALLOC(struct http_decoder_header, table->headers,
                                     table->header_size * 2);
            table->header_size *= 2;
            for (size_t i = table->header_index; i < table->header_size; i++) {
                header = &table->headers[i];
                memset(header, 0, sizeof(struct http_decoder_header));
            }
        }
        header = &table->headers[table->header_index];
        http_decoder_string_refer(&header->key, at, len);
        break;
    case HTTP_ITEM_HDRVAL:
        header = &table->headers[table->header_index];
        http_decoder_string_refer(&header->val, at, len);
        break;
    case HTTP_ITEM_BODY:
        http_decoder_string_refer(&table->body, at, len);
        break;
    default:
        abort();
        break;
    }
}

void
http_decoder_table_cache(struct http_decoder_table *table, enum http_item type)
{
    if (NULL == table) {
        return;
    }

    struct http_decoder_header *header = NULL;
    assert(table);

    switch (type) {
    case HTTP_ITEM_URI:
        http_decoder_string_cache(&table->uri);
        break;
    case HTTP_ITEM_STATUS:
        http_decoder_string_cache(&table->status);
        break;
    case HTTP_ITEM_METHOD:
        http_decoder_string_cache(&table->method);
        break;
    case HTTP_ITEM_VERSION:
        http_decoder_string_cache(&table->version);
        break;
    case HTTP_ITEM_HDRKEY:
        header = &table->headers[table->header_index];
        http_decoder_string_cache(&header->key);
        break;
    case HTTP_ITEM_HDRVAL:
        header = &table->headers[table->header_index];
        http_decoder_string_cache(&header->val);
        break;
    case HTTP_ITEM_BODY:
        http_decoder_string_cache(&table->body);
        break;
    default:
        abort();
        break;
    }
}

void
http_decoder_table_commit(struct http_decoder_table *table, enum http_item type)
{
    if (NULL == table) {
        return;
    }

    struct http_decoder_header *header = NULL;
    assert(table);

    switch (type) {
    case HTTP_ITEM_URI:
        http_decoder_string_commit(&table->uri);
        break;
    case HTTP_ITEM_STATUS:
        http_decoder_string_commit(&table->status);
        break;
    case HTTP_ITEM_METHOD:
        http_decoder_string_commit(&table->method);
        break;
    case HTTP_ITEM_VERSION:
        http_decoder_string_commit(&table->version);
        break;
    case HTTP_ITEM_HDRKEY:
        header = &table->headers[table->header_index];
        http_decoder_string_commit(&header->key);
        break;
    case HTTP_ITEM_HDRVAL:
        header = &table->headers[table->header_index];
        http_decoder_string_commit(&header->val);
        // inc index
        table->header_index++;
        break;
    case HTTP_ITEM_BODY:
        http_decoder_string_commit(&table->body);
        break;
    default:
        abort();
        break;
    }
}

void
http_decoder_table_reset(struct http_decoder_table *table, enum http_item type)
{
    if (NULL == table) {
        return;
    }

    struct http_decoder_header *header = NULL;
    assert(table);

    switch (type) {
    case HTTP_ITEM_URI:
        http_decoder_string_reset(&table->uri);
        break;
    case HTTP_ITEM_STATUS:
        http_decoder_string_reset(&table->status);
        break;
    case HTTP_ITEM_METHOD:
        http_decoder_string_reset(&table->method);
        break;
    case HTTP_ITEM_VERSION:
        http_decoder_string_reset(&table->version);
        break;
    case HTTP_ITEM_HDRKEY:
        header = &table->headers[table->header_index];
        http_decoder_string_reset(&header->key);
        break;
    case HTTP_ITEM_HDRVAL:
        header = &table->headers[table->header_index];
        http_decoder_string_reset(&header->val);
        break;
    case HTTP_ITEM_BODY:
        http_decoder_string_reset(&table->body);
        break;
    default:
        abort();
        break;
    }
}

void
http_decoder_table_dump(struct http_decoder_table *table)
{
    if (NULL == table) {
        return;
    }

    http_decoder_string_dump(&table->uri, "uri");
    http_decoder_string_dump(&table->status, "status");
    http_decoder_string_dump(&table->method, "method");
    http_decoder_string_dump(&table->version, "version");
    http_decoder_string_dump(&table->body, "body");

    for (size_t i = 0; i < table->header_size; i++) {
        struct http_decoder_header *header = &table->headers[i];
        if (NULL == header) {
            continue;
        }

        http_decoder_string_dump(&header->key, "key");
        http_decoder_string_dump(&header->val, "val");
    }
}

int
http_decoder_table_get_uri(struct http_decoder_table *table,
    struct hstring *out)
{
    if (NULL == table || NULL == out) {
        return -1;
    }

    return http_decoder_string_get(&table->uri, out);
}

int
http_decoder_table_get_method(struct http_decoder_table *table,
    struct hstring *out)
{
    if (NULL == table || NULL == out) {
        return -1;
    }

    return http_decoder_string_get(&table->method, out);
}

int
http_decoder_table_get_status(struct http_decoder_table *table,
    struct hstring *out)
{
    if (NULL == table || NULL == out) {
        return -1;
    }

    return http_decoder_string_get(&table->status, out);
}

int
http_decoder_table_get_version(struct http_decoder_table *table,
    struct hstring *out)
{
    if (NULL == table || NULL == out) {
        return -1;
    }

    return http_decoder_string_get(&table->version, out);
}

int
http_decoder_table_get_body(struct http_decoder_table *table,
    struct hstring *out)
{
    if (NULL == table || NULL == out) {
        return -1;
    }

    return http_decoder_string_get(&table->body, out);
}

int
http_decoder_table_get_header(struct http_decoder_table *table,
    struct hstring *key, struct http_header *header_array,
    size_t array_size)
{
    if (NULL == table || NULL == key->str || 0 == key->str_len) {
        return 0;
    }

    int header_cnt = 0;
    for (int i = 0; i < table->header_size && header_cnt < array_size; i++) {
        struct http_decoder_header *tmp_header = &table->headers[i];
        if (NULL == tmp_header) {
            continue;
        }

        if (http_decoder_string_state(&tmp_header->key) == STRING_STATE_COMMIT
            && http_decoder_string_state(&tmp_header->val) == STRING_STATE_COMMIT) {
            struct hstring tmp_key;
            http_decoder_string_get(&tmp_header->key, &tmp_key);

            if (tmp_key.str_len == key->str_len
                && (0 == strncasecmp(tmp_key.str, key->str, key->str_len))) {
                http_decoder_string_get(&tmp_header->val,
                                        &header_array[header_cnt++].val);
            }
        }
    }

    return header_cnt;
}

int
http_decoder_table_iter_header(struct http_decoder_table *table,
    struct http_header *header)
{
    if (NULL == table || NULL == header) {
        return -1;
    }

    if (table->header_iter >= table->header_size) {
        return 0;
    }

    struct http_decoder_header *tmp_header =
        &table->headers[table->header_iter++];
    if (tmp_header != NULL) {
        if (http_decoder_string_state(&tmp_header->key) == STRING_STATE_COMMIT
            && http_decoder_string_state(&tmp_header->val) == STRING_STATE_COMMIT) {
            http_decoder_string_get(&tmp_header->key, &header->key);
            http_decoder_string_get(&tmp_header->val, &header->val);
            return 1;
        }
    }

    header->key.str = NULL;
    header->key.str_len = 0;

    header->val.str = NULL;
    header->val.str_len = 0;

    return 0;
}