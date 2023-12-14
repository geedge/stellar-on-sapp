/*
 *  author:yangwei
 *  create time:2021-8-21
 *
 */



#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#include "../../src/http_decoder/http_decoder.h"

#ifdef __cplusplus
extern "C"
{


#include "cJSON.h"
#include "MESA_prof_load.h"

#include "stellar/utils.h"
#include "stellar/stellar.h"
#include "stellar/session_exdata.h"
#include "stellar/session_mq.h"

int commit_test_result_json(cJSON *node, const char *name);
}
#endif

#include "MESA/stream.h"

static int g_result_count = 1;
int g_test_exdata_idx = 0;
int g_l7_exdata_idx  = 0;
int g_test_app_plugin_id = 0;
int g_stellar_session_bridge_id = -1;

#define	MAX_PROTO_ID_NUM	10000
static char *g_proto_id2name[MAX_PROTO_ID_NUM];

struct http_decoder_test_context {
    int topic_id;
    int plugin_id;
};

// static int load_l7_protocol_mapper(const char *filename) {
//     int ret = 0, proto_id = 0;
//     char line[1024] = {0};
//     char type_name[32] = {0};
//     char proto_name[32] = {0};

//     memset(g_proto_id2name, 0, sizeof(g_proto_id2name));
// 	FILE *fp = fopen(filename, "r");
// 	if (NULL == fp) {
// 		printf("Open %s failed ...", filename);
// 		return -1;
// 	}

// 	memset(line, 0, sizeof(line));

// 	while ((fgets(line, sizeof(line), fp)) != NULL) {
// 		if (line[0] == '#' || line[0] == '\n' ||
//             line[0] == '\r' || line[0] == '\0') {
// 			continue;
// 		}
		
// 		ret = sscanf(line, "%30s %30s %d", type_name, proto_name, &proto_id);
// 		assert(ret == 3 && proto_id < MAX_PROTO_ID_NUM);

// 		g_proto_id2name[proto_id] = (char *)calloc(strlen(proto_name)+1, 1);
//         strcpy(g_proto_id2name[proto_id], proto_name);
// 		memset(line, 0, sizeof(line));
// 	}

// 	fclose(fp);
// 	fp = NULL;

// 	return ret;
// }

static void commit_test_result(cJSON *ctx, struct session *sess)
{
        assert(g_l7_exdata_idx >= 0  && ctx != NULL);
        struct http_request_line *label = (struct http_request_line *)session_get_ex_data(sess, g_l7_exdata_idx);;
        // if(label != NULL)
        // {
        //     int proto_ids[8];
        //     const char* proto_names[8];
        //     for(int i = 0; i < label->protocol_id_num; i++)
        //     {
        //         proto_ids[i] = (int)(label->protocol_id[i]);
        //         proto_names[i] = g_proto_id2name[proto_ids[i]];

        //     }
        //     cJSON *label_ids = cJSON_CreateIntArray(proto_ids, label->protocol_id_num);
        //     cJSON_AddItemToObject(ctx, "l7_label_id", label_ids);
        //     cJSON *label_names = cJSON_CreateStringArray(proto_names, label->protocol_id_num);
        //     cJSON_AddItemToObject(ctx, "l7_label_name", label_names);
        // }
        // else
        // {
        //         cJSON_AddStringToObject(ctx, "l7_label_id", "UNKNOWN");
        // }
        // unsigned char dir_flag;
        // int is_symmetric=session_is_symmetric(sess, &dir_flag);
        // if(is_symmetric)
        // {
        //    cJSON_AddStringToObject(ctx, "STREAM_DIR", "DOUBLE"); 
        // }
        // else if(dir_flag == SESSION_SEEN_C2S_FLOW)
        // {
        //     cJSON_AddStringToObject(ctx, "STREAM_DIR", "C2S");
        // }
        // else if(dir_flag == SESSION_SEEN_S2C_FLOW)
        // {
        //      cJSON_AddStringToObject(ctx, "STREAM_DIR", "S2C");
        // }
        // else
        // {
        //     assert(0);
        // }
        // if (ctx)
        // {
        //     char result_name[128] = "";
        //     sprintf(result_name, "APP_PROTO_IDENTIFY_RESULT_%d", g_result_count);
        //     commit_test_result_json(ctx, result_name);
        //     g_result_count += 1;
        // }
        return;
}

static int http_decoder_test_entry(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
    struct http_message *msg = (struct http_message *)data;

    printf("................http_decoder_test_entry................\n");
    struct http_request_line line;
    memset(&line, 0, sizeof(line));

    enum http_message_type msg_type = http_message_type(msg);
    switch (msg_type) {
    case HTTP_MESSAGE_REQ_LINE:
        http_message_get_request_line(msg, &line);
        break;
    default:
        break;
    }

    return 0;
}

extern "C" void *http_decoder_test_init(struct stellar *st)
{
    char l7_label_name[256] = "";
    char l7_bridge_name[256] = "";
    char l7_proto_name[256] = "";

    MESA_load_profile_string_def("./tsgconf/main.conf", "SYSTEM", "L7_LABEL_NAME", l7_label_name, sizeof(l7_label_name), "L7_PROTOCOL_LABEL");
	MESA_load_profile_string_def("./tsgconf/main.conf", "SYSTEM", "APP_BRIDGE_NAME", l7_bridge_name, sizeof(l7_bridge_name), "APP_BRIDGE");
	MESA_load_profile_string_def("./tsgconf/main.conf", "SYSTEM", "L7_PROTOCOL_FILE", l7_proto_name, sizeof(l7_proto_name), "./tsgconf/tsg_l7_protocol.conf");

    int topic_id = session_mq_get_topic_id(st, "HTTP_DECODER_MESSAGE");
    if (topic_id < 0) {
        printf("http_decoder_test_init: can't get http_decoder topic id !!!\n");	
		exit(-1); 
    }

    session_mq_subscribe_topic(st, topic_id, http_decoder_test_entry, NULL);
    printf("http_decoder_test_init OK!\n");	

    return NULL;
}

extern "C" void http_decoder_test_exit(void *ctx)
{
    if (NULL == ctx) {
        return;
    }

    FREE(ctx);
    printf("http_decoder_test_exit OK!\n");
}