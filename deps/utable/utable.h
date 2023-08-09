#pragma once

#include <stddef.h>
#include <uuid/uuid.h>

struct file_chunk
{
    uuid_t uuid;
    size_t offset;
    const char *chunk;
    size_t chunk_len;
};

struct utable;
struct utable *utable_new(void);
void utable_free(struct utable *table);

void utable_add_cstring(struct utable *table, const char *key, const char *value);
void utable_add_blob(struct utable *table, const char *key, const char *blob, size_t blob_sz);
void utable_add_integer_array(struct utable *table, const char *key, long long value_array[], size_t n_value);
void utable_delete_item(struct utable *table, const char *key);

enum utable_value_type
{
    utable_value_type_undefined=0,
    utable_value_type_cstring,
    utable_value_type_blob,
    utable_value_type_integer_array
};

void utable_reset_iter(struct utable *table);
const char *utable_next_key(struct utable *table);
enum utable_value_type utable_get_value_type(struct utable *table, const char *key);
int utable_get0_blob_value(struct utable *table, const char *key, char **value, size_t *value_len);
int utable_get0_cstring_value(struct utable *table, const char *key, char **value, size_t *value_len);
int utable_get0_integer_value_array(struct utable *table, const char *key, long long **value_array, size_t *n_value);

struct utable *utable_duplicate(struct utable *table);
int utable_merge(struct utable *dst, struct utable *src);

void utable_serialize(struct utable *table, char **blob, size_t *blob_len);
struct utable *utable_deserialize(char *bolb, size_t blob_len);

int utable_ipfix_exporter(struct utable *table, char **blob, size_t *blob_len);
int utable_json_export(struct utable *table, char **blob, size_t *blob_len);
int utable_msgpack_export(struct utable *table, char **blob, size_t *blob_len);