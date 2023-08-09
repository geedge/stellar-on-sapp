#include "utable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpack.h"
#include "uthash.h"
#include "b64.h"
#include "cJSON.h"

struct utable_item {
    char *key;
    enum utable_value_type value_type;
    union {
        char *cstring;
        struct
        {
            char *blob;
            size_t blob_sz;
        };
        struct
        {
            long long *value_array;
            size_t n_value;
        };
    };
    UT_hash_handle hh;
};

struct utable {
    struct utable_item *items;
    struct utable_item *iter;
};


struct utable *utable_new(void)
{
    struct utable *table = calloc(1, sizeof(struct utable));
    return table;
}

void utable_free(struct utable *table)
{
    struct utable_item *item, *tmp;
    HASH_ITER(hh, table->items, item, tmp) {
        HASH_DEL(table->items, item);
        free(item->key);
        switch (item->value_type) {
            case utable_value_type_cstring:
                free(item->cstring);
                break;
            case utable_value_type_blob:
                free(item->blob);
                break;
            case utable_value_type_integer_array:
                free(item->value_array);
                break;
            default:
                break;
        }
        free(item);
    }
    free(table);
}

void utable_add_cstring(struct utable *table, const char *key, const char *value)
{
    // check if key already exists
    struct utable_item *item;
    HASH_FIND_STR(table->items, key, item);
    if (item) {
        printf("ERR: key %s already exists\n", key);
        return;
    }

    item = calloc(1, sizeof(struct utable_item));
    item->key = strdup(key);
    item->value_type = utable_value_type_cstring;
    item->cstring = strdup(value);
    HASH_ADD_KEYPTR(hh, table->items, item->key, strlen(item->key), item);
}

void utable_add_blob(struct utable *table, const char *key, const char *blob, size_t blob_sz)
{
    // check if key already exists
    struct utable_item *item;
    HASH_FIND_STR(table->items, key, item);
    if (item) {
        printf("ERR: key %s already exists\n", key);
        return;
    }

    item = calloc(1, sizeof(struct utable_item));
    item->key = strdup(key);
    item->value_type = utable_value_type_blob;
    item->blob = malloc(blob_sz);
    memcpy(item->blob, blob, blob_sz);
    item->blob_sz = blob_sz;
    HASH_ADD_KEYPTR(hh, table->items, item->key, strlen(item->key), item);
}

void utable_add_integer_array(struct utable *table, const char *key, long long value_array[], size_t n_value)
{
    // check if key already exists
    struct utable_item *item;
    HASH_FIND_STR(table->items, key, item);
    if (item) {
        printf("ERR: key %s already exists\n", key);
        return;
    }

    item = calloc(1, sizeof(struct utable_item));
    item->key = strdup(key);
    item->value_type = utable_value_type_integer_array;
    item->value_array = malloc(sizeof(long long) * n_value);
    memcpy(item->value_array, value_array, sizeof(long long) * n_value);
    item->n_value = n_value;
    HASH_ADD_KEYPTR(hh, table->items, item->key, strlen(item->key), item);
}

void utable_delete_item(struct utable *table, const char *key)
{
    struct utable_item *item;
    HASH_FIND_STR(table->items, key, item);
    if (item) {
        HASH_DEL(table->items, item);
        free(item->key);
        switch (item->value_type) {
            case utable_value_type_cstring:
                free(item->cstring);
                break;
            case utable_value_type_blob:
                free(item->blob);
                break;
            case utable_value_type_integer_array:
                free(item->value_array);
                break;
            default:
                break;
        }
        free(item);
    }
}

void utable_reset_iter(struct utable *table)
{
    table->iter = NULL;
}

const char *utable_next_key(struct utable *table)
{
    if (table->iter == NULL) {
        table->iter = table->items;
    } else {
        table->iter = table->iter->hh.next;
    }
    if (table->iter == NULL) {
        return NULL;
    }
    return table->iter->key;
}

enum utable_value_type utable_get_value_type(const struct utable *table, const char *key)
{
    struct utable_item *item;
    HASH_FIND_STR(table->items, key, item);
    if (item) {
        return item->value_type;
    }
    return utable_value_type_undefined;
}

int utable_get0_blob_value(const struct utable *table, const char *key, char **value, size_t *value_len)
{
    struct utable_item *item;
    HASH_FIND_STR(table->items, key, item);
    if (item) {
        if (item->value_type == utable_value_type_blob) {
            char *blob = malloc(item->blob_sz);
            *value = blob;
            *value_len = item->blob_sz;
            memcpy(blob, item->blob, item->blob_sz);
            return 0;
        }
    }
    return -1;
}

int utable_get0_cstring_value(const struct utable *table, const char *key, char **value, size_t *value_len)
{
    struct utable_item *item;
    HASH_FIND_STR(table->items, key, item);
    if (item) {
        if (item->value_type == utable_value_type_cstring) {
            *value = strdup(item->cstring);
            *value_len = strlen(item->cstring);
            return 0;
        }
    }
    return -1;
}

int utable_get0_integer_value_array(const struct utable *table, const char *key, long long **value_array, size_t *n_value)
{
    struct utable_item *item;
    HASH_FIND_STR(table->items, key, item);
    if (item) {
        if (item->value_type == utable_value_type_integer_array) {
            *value_array = malloc(sizeof(long long) * item->n_value);
            memcpy(*value_array, item->value_array, sizeof(long long) * item->n_value);
            *n_value = item->n_value;
            return 0;
        }
    }
    return -1;
}

struct utable_item *utable_item_dup(const struct utable_item *item)
{
    struct utable_item *new_item = calloc(1, sizeof(struct utable_item));
    new_item->key = strdup(item->key);
    new_item->value_type = item->value_type;
    switch (item->value_type) {
        case utable_value_type_cstring:
            new_item->cstring = strdup(item->cstring);
            break;
        case utable_value_type_blob:
            new_item->blob = malloc(item->blob_sz);
            memcpy(new_item->blob, item->blob, item->blob_sz);
            new_item->blob_sz = item->blob_sz;
            break;
        case utable_value_type_integer_array:
            new_item->value_array = malloc(sizeof(long long) * item->n_value);
            memcpy(new_item->value_array, item->value_array, sizeof(long long) * item->n_value);
            new_item->n_value = item->n_value;
            break;
        default:
            break;
    }
    return new_item;
}

struct utable *utable_duplicate(const struct utable *table)
{
    struct utable *new_table = calloc(1, sizeof(struct utable));
    struct utable_item *item, *tmp;
    HASH_ITER(hh, table->items, item, tmp) {
        struct utable_item *new_item = utable_item_dup(item);
        HASH_ADD_KEYPTR(hh, new_table->items, new_item->key, strlen(new_item->key), new_item);
    }
    return new_table;
}

int utable_merge(struct utable *dst, const struct utable *src)
{
    struct utable_item *item, *tmp;
    HASH_ITER(hh, src->items, item, tmp) {
        struct utable_item *dst_item;
        HASH_FIND_STR(dst->items, item->key, dst_item);
        if (dst_item) {
            continue;
        }

        struct utable_item *new_item = utable_item_dup(item);
        HASH_ADD_KEYPTR(hh, dst->items, new_item->key, strlen(new_item->key), new_item);
    }
    return 0;
}

int utable_ipfix_exporter(const struct utable *table, char **blob, size_t *blob_len)
{
    return 0;  // todo
}

int utable_json_export(const struct utable *table, char **blob, size_t *blob_len)
{
    cJSON *cjson_root = cJSON_CreateObject();
    struct utable_item *item, *tmp;
    HASH_ITER(hh, table->items, item, tmp) {
        switch (item->value_type) {
            case utable_value_type_cstring:
                cJSON_AddStringToObject(cjson_root, item->key, item->cstring);
                break;
            case utable_value_type_blob: {
                char *enc = b64_encode((unsigned char *)item->blob, item->blob_sz);
                cJSON_AddStringToObject(cjson_root, item->key, enc);
                free(enc);
                }
                break;
            case utable_value_type_integer_array: {
                cJSON *cjson_array = cJSON_CreateArray();
                for (size_t i = 0; i < item->n_value; i++) {
                    cJSON *cjson_item = cJSON_CreateNumber(item->value_array[i]);
                    cJSON_AddItemToArray(cjson_array, cjson_item);
                }
                cJSON_AddItemToObject(cjson_root, item->key, cjson_array);
            }
                break;
            default:
                break;
        }
    }

    char *json_str = cJSON_PrintUnformatted(cjson_root);
    cJSON_Delete(cjson_root);
    *blob = json_str;
    *blob_len = strlen(json_str);
    return 0;
}

/*
    [{
        "key":<tag key string>
        "type":"i_arr", "c_str", or "blob"
        "value":<
    }]
*/
int utable_msgpack_export(const struct utable *table, char **blob, size_t *blob_len)
{
    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, blob, blob_len);
    mpack_start_array(&writer, HASH_COUNT(table->items));

    struct utable_item *item, *tmp;
    HASH_ITER(hh, table->items, item, tmp) {
        mpack_start_map(&writer, 3);
        mpack_write_cstr(&writer, "key");
        mpack_write_cstr(&writer, item->key);

        mpack_write_cstr(&writer, "type");
        switch (item->value_type) {
            case utable_value_type_cstring:
                mpack_write_cstr(&writer, "c_str");
                break;
            case utable_value_type_blob:
                mpack_write_cstr(&writer, "blob");
                break;
            case utable_value_type_integer_array:
                mpack_write_cstr(&writer, "i_arr");
                break;
            default:
                break;
        }

        mpack_write_cstr(&writer, "value");
        switch (item->value_type) {
            case utable_value_type_cstring:
                mpack_write_cstr(&writer, item->cstring);
                break;
            case utable_value_type_blob:
                mpack_write_bin(&writer, item->blob, item->blob_sz);
                break;
            case utable_value_type_integer_array:
                mpack_start_array(&writer, item->n_value);
                for (size_t i = 0; i < item->n_value; i++) {
                    mpack_write_i64(&writer, item->value_array[i]);
                }
                mpack_finish_array(&writer);
                break;
            default:
                break;
        }

        mpack_finish_map(&writer);
    }

    mpack_finish_array(&writer);
    if (mpack_writer_destroy(&writer) != mpack_ok) {
        printf("ERR mpack writer fieldtag_list_serialize destroy failed\n");
        return -1;
    }
    return 0;
}

void utable_serialize(const struct utable *table, char **blob, size_t *blob_len) {
    utable_msgpack_export(table, blob, blob_len);
}

char *get_cstring_from_mpack_node(mpack_node_t node)
{
    size_t len = mpack_node_strlen(node);
    char *str = malloc(len + 1);
    memcpy(str, mpack_node_str(node), len);
    str[len] = '\0';
    return str;
}

struct utable *utable_deserialize(const char *blob, size_t blob_len)
{
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, blob, blob_len);
    mpack_tree_parse(&tree);
    mpack_node_t root = mpack_tree_root(&tree);
    if (mpack_node_type(root) != mpack_type_array) {
        printf("ERR mpack root type is not array\n");
        return NULL;
    }
    size_t n_item = mpack_node_array_length(root);

    struct utable *table = utable_new();
    for (size_t i = 0; i < n_item; i++) {
        mpack_node_t item = mpack_node_array_at(root, i);
        mpack_node_t key = mpack_node_map_cstr(item, "key");
        mpack_node_t type = mpack_node_map_cstr(item, "type");
        mpack_node_t value = mpack_node_map_cstr(item, "value");
        char *type_str = get_cstring_from_mpack_node(type);

        struct utable_item *new_item = malloc(sizeof(struct utable_item));
        new_item->key = get_cstring_from_mpack_node(key);
    
        if (strcmp(type_str, "c_str") == 0) {
            new_item->value_type = utable_value_type_cstring;
            new_item->cstring = get_cstring_from_mpack_node(value);
        } else if (strcmp(type_str, "blob") == 0) {
            new_item->value_type = utable_value_type_blob;
            new_item->blob = malloc(mpack_node_bin_size(value));
            new_item->blob_sz = mpack_node_bin_size(value);
            memcpy(new_item->blob, mpack_node_bin_data(value), mpack_node_bin_size(value));
        } else if (strcmp(type_str, "i_arr") == 0) {
            new_item->value_type = utable_value_type_integer_array;
            new_item->n_value = mpack_node_array_length(value);
            new_item->value_array = malloc(sizeof(int64_t) * new_item->n_value);
            for (size_t j = 0; j < new_item->n_value; j++) {
                new_item->value_array[j] = mpack_node_i64(mpack_node_array_at(value, j));
            }
        } else {
            printf("ERR unknown type %s\n", type_str);
            mpack_tree_destroy(&tree);
            free(type_str);
            return NULL;
        }

        HASH_ADD_KEYPTR(hh, table->items, new_item->key, strlen(new_item->key), new_item);
        free(type_str);
    }

    mpack_tree_destroy(&tree);
    return table;
}
