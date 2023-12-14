/*
**********************************************************************************************
*	File: http_decoder_string.h
*   Description: 
*	Authors: LuWenPeng <luwenpeng@geedgenetworks.com>
*	Date:    2022-10-31
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

#include <stdlib.h>

#include "http_decoder_string.h"

static const char *string_state_to_desc(enum string_state state)
{
    switch (state) {
    case STRING_STATE_INIT:
        return "init";
        break;
    case STRING_STATE_REFER:
        return "refer";
        break;
    case STRING_STATE_CACHE:
        return "cache";
        break;
    case STRING_STATE_COMMIT:
        return "commit";
        break;
    default:
        return "unknown";
        break;
    }
}

void http_decoder_string_refer(struct http_decoder_string *rstr,
                                const char *at, size_t length)
{
    if (NULL == rstr) {
        return;
    }

    switch (rstr->state) {
        case STRING_STATE_INIT:
        case STRING_STATE_CACHE:
            rstr->refer.str = (char *)at;
            rstr->refer.str_len = length;
            break;
        default:
            abort();
            break;
    }

    rstr->state = STRING_STATE_REFER;
}

void http_decoder_string_cache(struct http_decoder_string *rstr)
{
    if (NULL == rstr) {
        return;
    }

    switch (rstr->state) {
        case STRING_STATE_REFER:
            if (rstr->refer.str_len > 0) {
                rstr->cache.str = (char *)realloc(rstr->cache.str, rstr->cache.str_len + rstr->refer.str_len);
                memcpy(rstr->cache.str + rstr->cache.str_len, rstr->refer.str, rstr->refer.str_len);
                rstr->cache.str_len += rstr->refer.str_len;

                rstr->refer.str = NULL;
                rstr->refer.str_len = 0;
            }
            break;
        default:
            abort();
            break;
    }

    rstr->state = STRING_STATE_CACHE;
}

void http_decoder_string_commit(struct http_decoder_string *rstr)
{
    if (NULL == rstr) {
        return;
    }

    switch (rstr->state) {
        case STRING_STATE_REFER:
            if (rstr->cache.str_len) {
            http_decoder_string_cache(rstr);

            rstr->commit.str = rstr->cache.str;
            rstr->commit.str_len = rstr->cache.str_len;
            // not overwrite rstr->cache.str
            } else {
            rstr->commit.str = rstr->refer.str;
            rstr->commit.str_len = rstr->refer.str_len;

            rstr->refer.str = NULL;
            rstr->refer.str_len = 0;
            }
            break;
        case STRING_STATE_CACHE:
            rstr->commit.str = rstr->cache.str;
            rstr->commit.str_len = rstr->cache.str_len;
            // not overwrite rstr->cache.str
            break;
        default:
            abort();
            break;
    }

    rstr->state = STRING_STATE_COMMIT;
}

void http_decoder_string_reset(struct http_decoder_string *rstr)
{
    assert(rstr);

    switch (rstr->state) {
    case STRING_STATE_INIT:
    case STRING_STATE_REFER:
    case STRING_STATE_CACHE:
    case STRING_STATE_COMMIT:
        safe_free(rstr->cache.str);
        memset(rstr, 0, sizeof(struct http_decoder_string));
        break;
    default:
        abort();
        break;
    }

    rstr->state = STRING_STATE_INIT;
}

enum string_state http_decoder_string_state(struct http_decoder_string *rstr)
{
    return rstr->state;
}

void http_decoder_string_get(struct http_decoder_string *rstr, struct hstring *out)
{
    if (NULL == rstr || NULL == out) {
        return;
    }

    if (http_decoder_string_state(rstr) == STRING_STATE_COMMIT) {
        out->str = rstr->commit.str;
        out->str_len = rstr->commit.str_len;
    } else {
        out->str = NULL;
        out->str_len = 0;
    }
}

void http_decoder_string_dump(struct http_decoder_string *rstr, const char *desc)
{
    if (NULL == rstr) {
        return;
    }

    char *refer_str = safe_dup(rstr->refer.str, rstr->refer.str_len);
    char *cache_str = safe_dup(rstr->cache.str, rstr->cache.str_len);
    char *commit_str = safe_dup(rstr->commit.str, rstr->commit.str_len);

    printf("%s: state: %s, refer: {len: %02zu, str: %s}, cache: {len: %02zu, str: %s}, commit: {len: %02zu, str: %s}\n",
           desc, string_state_to_desc(rstr->state),
           rstr->refer.str_len, refer_str,
           rstr->cache.str_len, cache_str,
           rstr->commit.str_len, commit_str);

    FREE(refer_str);
    FREE(cache_str);
    FREE(commit_str);
}