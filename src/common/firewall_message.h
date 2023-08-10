#pragma once

#include <stddef.h>

#define MESSAGE_MAGIC 0x12345678

#define	MAX_APP_ID_NUM	8
#define	APP_ID_MESSAE_TOPIC "TOPIC_APP_ID"

enum APP_IDENTIFY_ORIGIN
{
	ORIGIN_PROTO_IDENTIFY=0,
	ORIGIN_APP_SKETCH_USER_DEFINE,
	ORIGIN_PROTO_ENGINE,
	ORIGIN_APP_SKETCH_BUILT_IN,
	ORIGIN_MAX
};

struct app_id_message
{
	int magic;
	enum APP_IDENTIFY_ORIGIN origin;
	int app_id_num;
	unsigned int app_id[MAX_APP_ID_NUM];
	unsigned int surrogate_id[MAX_APP_ID_NUM];

};

#define	APP_LUA_RESULT_MESSAGE_TOPIC "TOPIC_APP_LUA_RESULT"
#define	SF_LOG_MESSAE_TOPIC "TOPIC_SF_LOG"

//using utable to store the lua result and sf log

#define	STATISTICS_RULE_MESSAGE_TOPIC "TOPIC_STATISTICS_RULE"
#define	SHAPING_RULE_MESSAGE_TOPIC "TOPIC_SHAPING_RULE"
#define	SERVICE_CHAIN_RULE_MESSAGE_TOPIC "TOPIC_SERVICE_CHAIN_RULE"
#define	INTERCEPT_RULE_MESSAGE_TOPIC "TOPIC_INTERCEPT_RULE"
#define	TROUBLE_SHOOTING_RULE_MESSAGE_TOPIC "TOPIC_TROUBLE_SHOOTING_RULE"
#define	APP_SIGNATURE_RULE_MESSAGE_TOPIC "TOPIC_APP_SIGNATURE_RULE"
#define	APP_PRE_SIGNATURE_RULE_MESSAGE_TOPIC "TOPIC_APP_PRE_SIGNATURE_RULE"

#define	MAX_RULE_ID_NUM	8
struct tsg_rule_message
{
	int magic;
	int rule_id_num;
	long long rule_id[];
};

#define	MONITOR_CAPTURE_INFO_MESSAGE_TOPIC "TOPIC_MONITOR_CAPTURE_INFO"
#define	MAX_VLAN_ID_NUM	8
struct capture_info_message
{
	int magic;
	long long rule_id;
	int capture_depth;
	int vlan_num;
	int vlan_id[];
};

#define	FILE_CHUNK_MESSAGE_TOPIC "TOPIC_FILE_CHUNK"
#include <uuid/uuid.h>
struct file_chunk
{
    uuid_t uuid;
    size_t offset;
    const char *chunk;
    size_t chunk_len;
};