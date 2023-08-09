
#include <gtest/gtest.h>

#include "b64.h"
#include "utable.h"
#include "cJSON.h"
#include "mpack.h"



void test_utable_assert_arr(const struct utable *table, const char *key, long long expected_arr[], size_t n_expected_arr)
{
    long long *arr = NULL;
    size_t n_arr = 0;
    EXPECT_EQ(utable_get_value_type(table, key), utable_value_type_integer_array);
    utable_get0_integer_value_array(table, key, &arr, &n_arr);
    EXPECT_EQ(n_arr, n_expected_arr);
    for (size_t i = 0; i < n_arr; i++) {
        EXPECT_EQ(expected_arr[i], arr[i]);
    }
    free(arr);
}

void test_utable_assert_str(const struct utable *table, const char *key, const char *expected_str)
{
    char *str = NULL;
    size_t str_len = 0;
    EXPECT_EQ(utable_get_value_type(table, key), utable_value_type_cstring);
    utable_get0_cstring_value(table, key, &str, &str_len);
    EXPECT_EQ(str_len, strlen(expected_str));
    EXPECT_EQ(strncmp(str, expected_str, str_len), 0);
    free(str);
}

void test_utable_assert_blob(const struct utable *table, const char *key, const char *expected_blob, size_t expected_blob_len)
{
    char *blob = NULL;
    size_t blob_len = 0;
    EXPECT_EQ(utable_get_value_type(table, key), utable_value_type_blob);
    utable_get0_blob_value(table, key, &blob, &blob_len);
    EXPECT_EQ(blob_len, expected_blob_len);
    EXPECT_EQ(memcmp(blob, expected_blob, blob_len), 0);
    free(blob);
}
// TEST(utable_test, new_del_when_empty)
// {
//     struct utable *table = utable_new();
//     ASSERT_TRUE(table != NULL);
//     utable_free(table);
// }

// TEST(utable_test, add_1_int_and_query)
// {
//     struct utable *table = utable_new();
//     long long int_array[] = { 1 };
//     utable_add_integer_array(table, "key", int_array, 1);
//     test_utable_assert_arr(table, "key", int_array, 1);

//     utable_free(table);
// }

// TEST(utable_test, add_2_int_and_query)
// {
//     struct utable *table = utable_new();
//     long long int_array[] = { 1, 2 };
//     utable_add_integer_array(table, "key", int_array, 2);
//     test_utable_assert_arr(table, "key", int_array, 2);

//     utable_free(table);
// }

// TEST(utable_test, add_string_and_query)
// {
//     struct utable *table = utable_new();
//     const char *str = "hello world";
//     utable_add_cstring(table, "key", str);
//     test_utable_assert_str(table, "key", str);

//     utable_free(table);
// }

// TEST(utable_test, add_blob_and_query)
// {
//     struct utable *table = utable_new();
//     const char *blob = "hello world";
//     size_t blob_len = strlen(blob);
//     utable_add_blob(table, "key", blob, blob_len);
//     test_utable_assert_blob(table, "key", blob, blob_len);

//     utable_free(table);
// }

// void test_utable_check_if_value_in_json(cJSON *tag_obj, const char *key, std::string &value)
// {
//     cJSON *tag_val = cJSON_GetObjectItem(tag_obj, key);
//     EXPECT_NE(tag_val, nullptr);
//     EXPECT_STREQ(tag_val->valuestring, value.c_str());
// }

// void test_utable_check_if_value_in_json(cJSON *tag_obj, const char *key, long long value[], size_t n_value)
// {
//     cJSON *tag_val = cJSON_GetObjectItem(tag_obj, key);
//     EXPECT_NE(tag_val, nullptr);
//     EXPECT_EQ(tag_val->type, cJSON_Array);
//     int arr_size = cJSON_GetArraySize(tag_val);
//     EXPECT_EQ(arr_size, n_value);
//     for (int i = 0; i < arr_size; i++) {
//         cJSON *arr_item = cJSON_GetArrayItem(tag_val, i);
//         EXPECT_EQ(arr_item->valueint, value[i]);
//     }
// }

// void test_utable_check_if_value_in_json(cJSON *tag_obj, const char *key, const char *value, size_t blob_len)
// {
//     cJSON *tag_val = cJSON_GetObjectItem(tag_obj, key);
//     EXPECT_NE(tag_val, nullptr);
//     // the tag_val is encoded in base64
//     size_t dec_size = 0;
//     unsigned char *dec = b64_decode_ex(tag_val->valuestring, strlen(tag_val->valuestring), &dec_size);
//     EXPECT_EQ(dec_size, blob_len);
//     EXPECT_EQ(memcmp(dec, value, blob_len), 0);

//     free(dec);
// }

// TEST(utable_test, export_json)
// {
//     struct utable *table = utable_new();
//     long long int_array[] = { 1, 2 };
//     const char *str = "hello world";
//     const char *blob = "bin hello world";
//     utable_add_integer_array(table, "key1", int_array, 2);
//     utable_add_cstring(table, "key2", str);
//     utable_add_blob(table, "key3", blob, strlen(blob));
    
//     // int utable_json_export(const struct utable *table, char **blob, size_t *blob_len);
//     char *json = NULL;
//     size_t json_len = 0;
//     EXPECT_EQ(utable_json_export(table, &json, &json_len), 0);
    
//     cJSON *root = cJSON_Parse(json);
//     test_utable_check_if_value_in_json(root, "key1", int_array, 2);
//     std::string expected_str(str);
//     test_utable_check_if_value_in_json(root, "key2", expected_str);
//     test_utable_check_if_value_in_json(root, "key3", blob, strlen(blob));
   
//     utable_free(table);
//     free(json);
//     cJSON_Delete(root);
// }

// char *test_get_cstring_from_mpack_node(mpack_node_t node)
// {
//     size_t len = mpack_node_strlen(node);
//     char *str = (char *)malloc(len + 1);
//     memcpy(str, mpack_node_str(node), len);
//     str[len] = '\0';
//     return str;
// }

// void test_utable_check_if_value_in_mpack(mpack_node_t item, const char *key, std::string &value)
// {
//     mpack_node_t key_node = mpack_node_map_cstr(item, "key");
//     mpack_node_t type_node = mpack_node_map_cstr(item, "type");
//     mpack_node_t value_node = mpack_node_map_cstr(item, "value");

//     char *key_tmp = test_get_cstring_from_mpack_node(key_node);
//     char *type_tmp = test_get_cstring_from_mpack_node(type_node);
//     char *value_tmp = test_get_cstring_from_mpack_node(value_node);
//     EXPECT_STREQ(key_tmp, key);
//     EXPECT_STREQ(type_tmp, "c_str");
//     EXPECT_STREQ(value_tmp, value.c_str());

//     free(key_tmp);
//     free(type_tmp);
//     free(value_tmp);
// }

// void test_utable_check_if_value_in_mpack(mpack_node_t item, const char *key, long long value[], size_t n_value)
// {
//     mpack_node_t key_node = mpack_node_map_cstr(item, "key");
//     mpack_node_t type_node = mpack_node_map_cstr(item, "type");
//     mpack_node_t value_node = mpack_node_map_cstr(item, "value");
//     char *key_tmp = test_get_cstring_from_mpack_node(key_node);
//     char *type_tmp = test_get_cstring_from_mpack_node(type_node);

//     EXPECT_STREQ(key_tmp, key);
//     EXPECT_STREQ(type_tmp, "i_arr");
//     EXPECT_EQ(mpack_node_array_length(value_node), n_value);
//     for (size_t i = 0; i < n_value; i++) {
//         mpack_node_t tmp_v_node = mpack_node_array_at(value_node, i);
//         EXPECT_EQ(mpack_node_i64(tmp_v_node), value[i]);
//     }
//     free(key_tmp);
//     free(type_tmp);
// }

// void test_utable_check_if_value_in_mpack(mpack_node_t item, const char *key, const char *value, size_t blob_len)
// {
//     mpack_node_t key_node = mpack_node_map_cstr(item, "key");
//     mpack_node_t type_node = mpack_node_map_cstr(item, "type");
//     mpack_node_t value_node = mpack_node_map_cstr(item, "value");
//     char *key_tmp = test_get_cstring_from_mpack_node(key_node);
//     char *type_tmp = test_get_cstring_from_mpack_node(type_node);
//     EXPECT_STREQ(key_tmp, key);
//     EXPECT_STREQ(type_tmp, "blob");
//     EXPECT_EQ(mpack_node_bin_size(value_node), blob_len);
//     EXPECT_EQ(memcmp(mpack_node_bin_data(value_node), value, blob_len), 0);

//     free(key_tmp);
//     free(type_tmp);
// }

// TEST(utable_test, export_mpack)
// {
//     struct utable *table = utable_new();
//     long long int_array[] = { 1, 2 };
//     const char *str = "hello world";
//     const char *blob = "bin hello world";
//     utable_add_integer_array(table, "key1", int_array, 2);
//     utable_add_cstring(table, "key2", str);
//     utable_add_blob(table, "key3", blob, strlen(blob));

//     char *mpack = NULL;
//     size_t mpack_len = 0;
//     EXPECT_EQ(utable_msgpack_export(table, &mpack, &mpack_len), 0);

//     // decode mpack

//     mpack_tree_t tree;
//     mpack_tree_init_data(&tree, mpack, mpack_len);
//     mpack_tree_parse(&tree);
//     mpack_node_t root = mpack_tree_root(&tree);
//     size_t n_item = mpack_node_array_length(root);
//     EXPECT_EQ(n_item, 3);

//     test_utable_check_if_value_in_mpack(mpack_node_array_at(root, 0), "key1", int_array, 2);
//     std::string expected_str(str);
//     test_utable_check_if_value_in_mpack(mpack_node_array_at(root, 1), "key2", expected_str);
//     test_utable_check_if_value_in_mpack(mpack_node_array_at(root, 2), "key3", blob, strlen(blob));

//     utable_free(table);
//     free(mpack);
//     mpack_tree_destroy(&tree);
// }


// TEST(utable_test, query_on_wrong_key)
// {
//     struct utable *table = utable_new();
    
//     char *value;
//     size_t value_len;
//     EXPECT_EQ(utable_get0_blob_value(table, "key", &value, &value_len), -1);
//     EXPECT_EQ(utable_get0_cstring_value(table, "key", &value, &value_len), -1);
//     long long *value_array;
//     size_t n_value;
//     EXPECT_EQ(utable_get0_integer_value_array(table, "key", &value_array, &n_value), -1);
//     EXPECT_EQ(utable_get_value_type(table, "key"), utable_value_type_undefined);

//     utable_free(table);
// }

// TEST(utable_test, add_on_dup_key)
// {
//     struct utable *table = utable_new();
//     long long int_array[] = { 1, 2 };
//     const char *str = "hello world";
//     utable_add_integer_array(table, "key", int_array, 2);
//     utable_add_cstring(table, "key", str);
    
//     test_utable_assert_arr(table, "key", int_array, 2);

//     utable_free(table);
// }

// TEST(utable_test, iter_and_query)
// {
//     struct utable *table = utable_new();
    
//     long long int_array[] = { 1, 2 };
//     const char *str = "hello world";
//     utable_add_integer_array(table, "key1", int_array, 2);
//     utable_add_cstring(table, "key2", str);
//     utable_add_integer_array(table, "key3", int_array, 2);

//     EXPECT_STREQ(utable_next_key(table), "key1");
//     EXPECT_STREQ(utable_next_key(table), "key2");
//     EXPECT_STREQ(utable_next_key(table), "key3");
//     EXPECT_EQ(utable_next_key(table), nullptr);
//     utable_reset_iter(table);
//     EXPECT_STREQ(utable_next_key(table), "key1");
//     EXPECT_STREQ(utable_next_key(table), "key2");
//     EXPECT_STREQ(utable_next_key(table), "key3");
//     EXPECT_EQ(utable_next_key(table), nullptr);

//     utable_free(table);
// }

// TEST(utable_test, dup_empty)
// {
//     struct utable *table = utable_new();

//     struct utable *dup_table = utable_duplicate(table);
//     EXPECT_EQ(utable_next_key(dup_table), nullptr);

//     utable_free(table);
//     utable_free(dup_table);
// }

// TEST(utable_test, dup_and_query)
// {
//     struct utable *table = utable_new();
    
//     const char *str = "hello world";
//     utable_add_cstring(table, "key1", str);
//     long long int_array[] = { 1, 2 };
//     utable_add_integer_array(table, "key2", int_array, 2);
//     const char *blob = "bin hello world";
//     utable_add_blob(table, "key3", blob, strlen(blob));

//     struct utable *dup_table = utable_duplicate(table);
//     test_utable_assert_arr(dup_table, "key2", int_array, 2);
//     test_utable_assert_str(dup_table, "key1", str);
//     test_utable_assert_blob(dup_table, "key3", blob, strlen(blob));

//     utable_free(table);
//     utable_free(dup_table);
// }

// TEST(utable_test, merge_empty_to_empty)
// {
//     struct utable *table = utable_new();
//     struct utable *table2 = utable_new();
//     utable_merge(table, table2);
//     EXPECT_EQ(utable_next_key(table), nullptr);

//     utable_free(table);
//     utable_free(table2);
// }

// TEST(utable_test, merge_all_new)
// {
//     struct utable *table = utable_new();
//     struct utable *table2 = utable_new();
    
//     const char *str = "hello world";
//     utable_add_cstring(table, "key1", str);
//     long long int_array[] = { 1, 2 };
//     utable_add_integer_array(table, "key2", int_array, 2);

//     const char *blob = "bin hello world";
//     utable_add_blob(table2, "key on tbl2", blob, strlen(blob));

//     utable_merge(table2, table);
//     test_utable_assert_arr(table2, "key2", int_array, 2);
//     test_utable_assert_str(table2, "key1", str);
//     test_utable_assert_blob(table2, "key on tbl2", blob, strlen(blob));

//     utable_free(table);
//     utable_free(table2);
// }

// TEST(utable_test, merge_all_skip)
// {
//     struct utable *table = utable_new();
//     const char *str = "hello world";
//     utable_add_cstring(table, "key1", str);
//     long long int_array[] = { 1, 2 };
//     utable_add_integer_array(table, "key2", int_array, 2);
//     struct utable *table2 = utable_duplicate(table);

//     utable_merge(table2, table);

//     EXPECT_STREQ(utable_next_key(table2), "key1");
//     EXPECT_STREQ(utable_next_key(table2), "key2");
//     EXPECT_EQ(utable_next_key(table2), nullptr);

//     utable_free(table);
//     utable_free(table2);
// }

// TEST(utable_test, merge_some_skip_some_new)
// {
//     struct utable *table = utable_new();    
//     struct utable *table2 = utable_new();

//     const char *str = "val on tbl1";
//     utable_add_cstring(table, "key_share", str);
//     const char *str2 = "val on tbl2";
//     utable_add_cstring(table2, "key_share", str2);
//     const char *str3 = "val on tbl1";
//     utable_add_cstring(table, "key1", str3);

//     utable_merge(table2, table);

//     test_utable_assert_str(table2, "key_share", str2);
//     test_utable_assert_str(table2, "key1", str3);

//     utable_free(table);
//     utable_free(table2);
// }

TEST(utable_test, serialize_and_deserialize)
{
    struct utable *table = utable_new();
    long long int_array[] = { 1, 2 };
    const char *str = "hello world";
    const char *blob = "bin hello world";
    utable_add_integer_array(table, "key1", int_array, 2);
    utable_add_cstring(table, "key2", str);
    utable_add_blob(table, "key3", blob, strlen(blob));

    char *blob_out;
    size_t blob_size;
    utable_serialize(table, &blob_out, &blob_size);

    struct utable *table2 = utable_deserialize(blob_out, blob_size);
    test_utable_assert_arr(table2, "key1", int_array, 2);
    test_utable_assert_str(table2, "key2", str);
    test_utable_assert_blob(table2, "key3", blob, strlen(blob));

    utable_free(table);
    free(blob_out);
    utable_free(table2);
}

TEST(utable_test, serialize_empty_and_deserialize)
{
    struct utable *table = utable_new();
    
    char *blob_out;
    size_t blob_size;
    utable_serialize(table, &blob_out, &blob_size);

    struct utable *table2 = utable_deserialize(blob_out, blob_size);

    ASSERT_TRUE(table2 != NULL);
    ASSERT_EQ(utable_next_key(table2), nullptr);

    utable_free(table);
    free(blob_out);
    utable_free(table2);
}

int main(int argc, char *argv[]) 
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}