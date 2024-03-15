#include "stellar/session.h"
#include "stellar/stellar.h"
#include "stellar/utils.h"
#include "stellar/session_exdata.h"
#include "stellar/session_mq.h"
#include "session_manager.h"
#include "stellar_internal.h"

#include "uthash/utarray.h"
#include "uthash/utlist.h"

struct session_exdata_schema
{
	char *name;
	session_ex_free *free_func;
	void *arg;
	int idx;
};

static void session_exdata_met_copy(void *_dst, const void *_src)
{
    struct session_exdata_schema *dst = (struct session_exdata_schema *)_dst, *src = (struct session_exdata_schema *)_src;
    dst->free_func = src->free_func;
    dst->arg = src->arg;
    dst->idx = src->idx;
    dst->name = src->name ? strdup(src->name) : NULL;
}

static void session_exdata_met_dtor(void *_elt)
{
    struct session_exdata_schema *elt = (struct session_exdata_schema *)_elt;
    if (elt->name)
        FREE(elt->name);
}

UT_icd session_exdata_meta_icd = {sizeof(struct session_exdata_schema), NULL, session_exdata_met_copy, session_exdata_met_dtor};


int stellar_session_get_ex_new_index(struct stellar *st, const char *name, session_ex_free *free_func,void *arg)
{
	if(st->session_exdata_schema_array == NULL)
	{
		utarray_new(st->session_exdata_schema_array, &session_exdata_meta_icd);
	}
	if(st->session_exdata_schema_array == NULL)return -1;
	unsigned int len = utarray_len(st->session_exdata_schema_array);
	struct session_exdata_schema *t_schema;
	for(unsigned int i = 0; i < len; i++)
	{
		t_schema = (struct session_exdata_schema *)utarray_eltptr(st->session_exdata_schema_array, i);
		if(strcmp(t_schema->name, name) == 0)
		{
			return t_schema->idx;
		}
	}
	struct session_exdata_schema new_schema;
	new_schema.free_func=free_func;
	new_schema.name=(char *)name;
	new_schema.idx=len;
	new_schema.arg=arg;
	utarray_push_back(st->session_exdata_schema_array, &new_schema);
	return new_schema.idx;
}

struct session_exdata_runtime
{
	void *data;
};

int session_set_ex_data(struct session *sess, int idx, void *ex_ptr)
{
	if(sess->ex_data_rt == NULL)return -1;
	if(sess->st->session_exdata_schema_array == NULL)return -1;
	unsigned int len=utarray_len(sess->st->session_exdata_schema_array);
	if(len < (unsigned int)idx)return -1;
	struct session_exdata_runtime* exdata = (struct session_exdata_runtime*)(sess->ex_data_rt+idx);
	exdata->data = ex_ptr;
	return 0;
}

void *session_get_ex_data(struct session *sess, int idx)
{
	if(sess->ex_data_rt == NULL)return NULL;
	if(sess->st->session_exdata_schema_array==NULL)return NULL;
	unsigned int len = utarray_len(sess->st->session_exdata_schema_array);
	if(len < (unsigned int)idx)return NULL;
	struct session_exdata_runtime* exdata = (struct session_exdata_runtime*)(sess->ex_data_rt+idx);
	return exdata->data;
}

typedef struct session_mq
{
	int topic_id;
    void *message;
	size_t message_len;
    struct session_mq *next, *prev;
}session_mq;

typedef struct session_mq_subscriber 
{
    on_msg_cb_func *sub_cb;
    void *cb_arg;
    struct session_mq_subscriber *next, *prev;
}session_mq_subscribers;

struct session_mq_topic_schema
{
	char *topic_name;
	msg_free_cb_func *free_cb;
	void *cb_arg;
	int topic_id;
	int sub_cnt;
	struct session_mq_subscriber *subscribers; 
};

static void session_mq_topic_schema_copy(void *_dst, const void *_src)
{
    struct session_mq_topic_schema *dst = (struct session_mq_topic_schema *)_dst,
                                   *src = (struct session_mq_topic_schema *)_src;
    dst->subscribers = src->subscribers;
    dst->free_cb = src->free_cb;
    dst->cb_arg = src->cb_arg;
    dst->topic_id = src->topic_id;
    dst->sub_cnt = src->sub_cnt;
    dst->topic_name = src->topic_name ? strdup(src->topic_name) : NULL;
}

static void session_mq_topic_schema_dtor(void *_elt)
{
    struct session_mq_topic_schema *elt = (struct session_mq_topic_schema *)_elt;
    if (elt->topic_name)
        FREE(elt->topic_name);
    // FREE(elt); // free the item
}

UT_icd session_mq_topic_schema_icd = {sizeof(struct session_mq_topic_schema), NULL, session_mq_topic_schema_copy, session_mq_topic_schema_dtor};


void session_mq_free(struct session_mq *head)
{
	struct session_mq *elt, *tmp;
    DL_FOREACH_SAFE(head, elt, tmp)
    {
        DL_DELETE(head, elt);
        free(elt);
    }
	free(head);
}



int session_mq_get_topic_id(struct stellar *st, const char *topic_name)
{
	if(topic_name == NULL || st == NULL || st->session_mq_schema_array == NULL)return -1;
	unsigned int len = utarray_len(st->session_mq_schema_array);
	struct session_mq_topic_schema *t_schema;
	for(unsigned int i = 0; i < len; i++)
	{
		t_schema = (struct session_mq_topic_schema *)utarray_eltptr(st->session_mq_schema_array, i);
		if(strcmp(t_schema->topic_name, topic_name) == 0)
		{
			return i;
		}
	}	
	return -1;
}

int session_mq_update_topic(struct stellar *st, int topic_id, msg_free_cb_func *free_cb, void *cb_arg)
{
	if(st->session_mq_schema_array == NULL)return -1;
	unsigned int len = utarray_len(st->session_mq_schema_array);
	if(len < (unsigned int)topic_id)return -1;
	struct session_mq_topic_schema *t_schema = (struct session_mq_topic_schema *)utarray_eltptr(st->session_mq_schema_array, (unsigned int)topic_id);
	if(t_schema == NULL)return -1;
	t_schema->free_cb=free_cb;
	t_schema->cb_arg=cb_arg;
	return 0;
}

int session_mq_create_topic(struct stellar *st, const char *topic_name, msg_free_cb_func *free_cb, void *cb_arg)
{
	if(st->session_mq_schema_array == NULL)
	{
		utarray_new(st->session_mq_schema_array, &session_mq_topic_schema_icd);
	}
	unsigned int len = utarray_len(st->session_mq_schema_array);
	if(session_mq_get_topic_id(st, topic_name) >= 0)
	{
		return -1;
	}
	struct session_mq_topic_schema t_schema;
	t_schema.free_cb=free_cb;
	t_schema.topic_name=(char *)topic_name;
	t_schema.topic_id=len;
	t_schema.cb_arg=cb_arg;
	t_schema.subscribers=NULL;
	t_schema.sub_cnt=0;
	utarray_push_back(st->session_mq_schema_array, &t_schema);
	st->topic_num+=1;
	return t_schema.topic_id;	
}

int session_mq_destroy_topic(struct stellar *st, int topic_id)
{
	if(st->session_mq_schema_array==NULL)return 0;
    unsigned int len = utarray_len(st->session_mq_schema_array);
    if (len <= (unsigned int)topic_id)
        return -1;
    struct session_mq_topic_schema *topic =
        (struct session_mq_topic_schema *)utarray_eltptr(st->session_mq_schema_array, (unsigned int)topic_id);
    struct session_mq_subscriber *sub_elt, *sub_tmp;

    if (topic)
    {
        DL_FOREACH_SAFE(topic->subscribers, sub_elt, sub_tmp)
        {
            DL_DELETE(topic->subscribers, sub_elt);
            free(sub_elt);
        }
    }
    return 0; // success
}



int session_mq_publish_message(struct session *sess, int topic_id, void *data)
{
	if(sess->st->session_mq_schema_array==NULL)return -1;
    unsigned int len = utarray_len(sess->st->session_mq_schema_array);
    if (len <= (unsigned int)topic_id)return -1;
	struct session_mq *node= CALLOC(struct session_mq,1);
	node->topic_id = topic_id;
	node->message = data;
	DL_APPEND(sess->mq, node);
	return 0;
}

int session_mq_ignore_message(struct session *sess, int topic_id, int sub_id)
{
	if(topic_id < 0 || sub_id < 0)return -1;
	if(topic_id >= sess->st->topic_num)return -1;
	struct session_mq_topic_schema *topic = (struct session_mq_topic_schema *)utarray_eltptr(sess->st->session_mq_schema_array, (unsigned int)topic_id);
	if(sub_id > topic->sub_cnt)return -1;
	if(sess->mq_sub_status)
	{
		sess->mq_sub_status[topic_id+(sub_id-1)]=0;
		return 0;
	}
	else
		return -1;
}

int session_mq_subscribe_topic(struct stellar *st, int topic_id, on_msg_cb_func *sub_cb, void *cb_arg)
{
	if(st->session_mq_schema_array==NULL)return -1;
    unsigned int len = utarray_len(st->session_mq_schema_array);
    if (len <= (unsigned int)topic_id)return -1;
    struct session_mq_topic_schema *topic = (struct session_mq_topic_schema *)utarray_eltptr(st->session_mq_schema_array, (unsigned int)topic_id);
	struct session_mq_subscriber *new_subscriber = CALLOC(struct session_mq_subscriber,1);
    new_subscriber->sub_cb = sub_cb;
    new_subscriber->cb_arg = cb_arg;
	DL_APPEND(topic->subscribers, new_subscriber);
	topic->sub_cnt+=1;
	st->sub_num+=1;
	return topic->sub_cnt;
}


int stellar_get_session_mq_topic_cnt(struct stellar *st)
{
	if(st->session_exdata_schema_array)
	{
		return utarray_len(st->session_mq_schema_array);
	}
	else
		return 0;
}

int stellar_get_session_mq_subscriber_cnt(struct stellar *st, int topic_id)
{
	if(st->session_exdata_schema_array==NULL)return -1;
	unsigned int len = utarray_len(st->session_mq_schema_array);
	if (len <= (unsigned int)topic_id)return -1;
	struct session_mq_topic_schema *topic = (struct session_mq_topic_schema *)utarray_eltptr(st->session_mq_schema_array, (unsigned int)topic_id);
	return topic->sub_cnt;
}

static struct session_exdata_runtime  *session_new_exdata_rt(struct stellar *st)
{
	struct session_exdata_runtime *exdata_rt = NULL;
	if(st->session_exdata_schema_array==NULL)return NULL;
	unsigned int len = utarray_len(st->session_exdata_schema_array);
	if(len > 0)
	{
		exdata_rt=CALLOC(struct session_exdata_runtime, len);
	}
	return exdata_rt;
}

static void session_free_exdata_runtime(struct session* sess, struct session_exdata_runtime *exdata_rt)
{
	if(exdata_rt==NULL)return;
	if(sess->st->session_exdata_schema_array==NULL)return;
	unsigned int len=utarray_len(sess->st->session_exdata_schema_array);
	for (unsigned int i = 0; i < len; i++)
	{
		struct session_exdata_runtime *exdata = (struct session_exdata_runtime *)(exdata_rt + i);
		struct session_exdata_schema *schema = (struct session_exdata_schema *)utarray_eltptr(sess->st->session_exdata_schema_array, i);
		if (exdata)
		{
			if (schema->free_func && exdata->data)
			{
				schema->free_func(sess, i, exdata->data, schema->arg);
			}
		}
	}
}


typedef struct session_event
{
	struct stellar *st;
	struct session *sess;
	session_event_cb_func *session_event_cb;
	void *cb_arg;
	struct timeval timeout;
	int events;
	unsigned short is_delete;
	unsigned short is_intrinsic;
	struct session_event *next, *prev;
}session_event;


void session_dispatch(struct session *sess, enum session_state state,  struct packet *pkt)
{
	struct session_event  *intrinsic_events = sess->intrinsic_events;
	struct session_event *p = NULL;
	if(sess->st->plugin_specs_array == NULL)return;
	if(sess->st->intrinsic_session_event_schema_array == NULL)return;
	unsigned int len = utarray_len(sess->st->intrinsic_session_event_schema_array);
	sess->state = state;
	// TODO: improve state to events logic 
	int events=(1<<state|(sess->type==SESSION_TYPE_TCP?SESS_EV_TCP:SESS_EV_UDP));
	for(unsigned i = 0; i < len; i++)
	{
		p = (intrinsic_events+i);
		if(p->session_event_cb != NULL && (p->events & events) && (p->is_delete == 0))
		{
			p->session_event_cb(sess, events, pkt, p->cb_arg);
		}
	}

	session_defer_loop(sess);
	return;
}

static struct session_event* session_intrinsic_events_init(struct stellar *st, struct session *sess)
{
	if(st->intrinsic_session_event_schema_array == NULL)return NULL;
	struct session_event *intrinsic_events = CALLOC(struct session_event, utarray_len(st->intrinsic_session_event_schema_array));
	for(unsigned i = 0; i < utarray_len(st->intrinsic_session_event_schema_array); i++)
	{
		intrinsic_events[i] = *(struct session_event *)utarray_eltptr(st->intrinsic_session_event_schema_array, i);
	}
	return intrinsic_events;
}

struct session *session_new(struct stellar *st, enum session_type type, int thread_id)
{
	if(type!=SESSION_TYPE_TCP && type!=SESSION_TYPE_UDP)return NULL;
	struct session *sess = CALLOC(struct session, 1);
	sess->st = st;
	sess->type = type;
	sess->state = SESSION_STATE_OPENING;
	sess->session_direction=-1;
	sess->mq = NULL;
	sess->mq_sub_status=CALLOC(char, st->topic_num*st->sub_num);
	memset(sess->mq_sub_status, 1, st->topic_num*st->sub_num);
	sess->ex_data_rt = session_new_exdata_rt(st);
	sess->ev_list=NULL;
	sess->intrinsic_events=session_intrinsic_events_init(st, sess);
	return sess;

}

inline struct packet *session_get_current_packet(struct session *sess)
{
	return sess->cur_pkt;
}

inline void session_set_current_packet(struct session *sess, struct packet *pkt, session_packet_free *free_cb, void *arg)
{
	sess->cur_pkt = pkt;
	sess->cur_pkt_free_arg = arg;
	sess->cur_pkt_free_cb = free_cb;
}

void session_free(struct session *sess)
{
	if(sess==NULL)return;
	if(sess->mq != NULL)
	{
		session_mq_free(sess->mq);
	}
	if(sess->mq_sub_status != NULL)
	{
		FREE(sess->mq_sub_status);
	}
	session_free_exdata_runtime(sess, sess->ex_data_rt);
	if(sess->cur_pkt)
	{
		if(sess->cur_pkt_free_cb)
		{
			sess->cur_pkt_free_cb(sess, sess->cur_pkt, sess->cur_pkt_free_arg);
		}
	}
	FREE(sess->ex_data_rt);
	if(sess->intrinsic_events)
	{
		FREE(sess->intrinsic_events);
	}
	FREE(sess);
}

UT_icd session_event_icd = {sizeof(struct session_event), NULL, NULL, NULL};

struct session_event *session_event_new(struct stellar *st, struct session *sess, int events, session_event_cb_func *cb, void *cb_arg)
{
	struct session_event *event = CALLOC(struct session_event, 1);
	event->events = events;
	event->session_event_cb = cb;
	event->cb_arg = cb_arg;
	event->sess = sess;
	event->st = st;
	return event;
}

void session_event_free(struct session_event *ev)
{
	if(ev==NULL)return;
	if(ev->is_intrinsic)
	{
		ev->is_delete=1;
	}
	else
	{
		FREE(ev);
	}
	return;
}

int session_event_add(struct session_event *ev, const struct timeval *timeout)
{
	// TODO: add to sapp stream timer
	if(timeout)
	{
		ev->timeout = *timeout;
	}
	if(ev->sess)
	{
		DL_APPEND(ev->sess->ev_list, ev);
	}
	return 0;
}


int session_event_assign(struct session_event *ev, struct stellar *st, struct session *sess, int events, session_event_cb_func *cb, void *cb_arg)
{
	if(ev==NULL)return -1;
	ev->events = events;
	ev->session_event_cb = cb;
	ev->cb_arg = cb_arg;
	ev->sess = sess;
	ev->st = st;
	return 0;
}

int session_event_del(struct session_event *ev)
{
	if(ev->is_delete==1)return 0;
	ev->is_delete=1;

	if(ev->is_intrinsic==0 && (ev->prev != NULL || ev->next != NULL))
	{
		DL_DELETE(ev->sess->ev_list, ev);
		ev->prev = NULL;
		ev->next = NULL;
	}
	return 0;
}

int stellar_plugin_register(struct stellar *st, int events, session_event_cb_func *cb, void *cb_arg)
{
	struct session_event event={.st=st, .sess=NULL, .session_event_cb=cb, .cb_arg=cb_arg, .timeout={0,0}, .events=events, .is_delete=0, .is_intrinsic=1,  .next=NULL, .prev=NULL};
	if(st->intrinsic_session_event_schema_array == NULL)
	{
		utarray_new(st->intrinsic_session_event_schema_array, &session_event_icd);
	}
	unsigned int len = utarray_len(st->intrinsic_session_event_schema_array);
	utarray_push_back(st->intrinsic_session_event_schema_array, &event);
	return len;
}

struct session_event *session_get_intrinsic_event(struct session *sess, int plugin_id)
{
	if(sess->st->intrinsic_session_event_schema_array == NULL)return NULL;
	unsigned int len = utarray_len(sess->st->intrinsic_session_event_schema_array);
	if(plugin_id < 0 || plugin_id >= (int)len)return NULL;
	struct session_event*intrinsic_events=sess->intrinsic_events;
	return (struct session_event *)(intrinsic_events+plugin_id);
}





void session_defer_loop(struct session *sess)
{

	struct session_mq *mq_elt=NULL, *mq_tmp=NULL;
	struct session_mq_subscriber *sub_elt, *sub_tmp;
	struct session_mq_topic_schema *topic;

	struct session_event *ev=NULL, *ev_tmp=NULL;
	int events=(1<<sess->state|(sess->type==SESSION_TYPE_TCP?SESS_EV_TCP:SESS_EV_UDP));
	const struct packet *pkt= session_get0_current_packet(sess);

    while (sess->mq != NULL || sess->ev_list != NULL)
    {
        DL_FOREACH_SAFE(sess->mq, mq_elt, mq_tmp)
        {
            topic = (struct session_mq_topic_schema *)utarray_eltptr(sess->st->session_mq_schema_array,
                                                                    (unsigned int)(mq_elt->topic_id));
            if (topic)
            {
				int cur_sub_idx = 0;
                DL_FOREACH_SAFE(topic->subscribers, sub_elt, sub_tmp)
                {
					if(sess->mq_sub_status[(mq_elt->topic_id)+cur_sub_idx] == 1)
					{
     	            	sub_elt->sub_cb(sess, mq_elt->topic_id, mq_elt->message, sub_elt->cb_arg);
					}
					cur_sub_idx++;
                }
                if (topic->free_cb)
                {
                    topic->free_cb(mq_elt->message, topic->cb_arg);
                }
            }
            DL_DELETE(sess->mq, mq_elt);
            free(mq_elt);
        }

        DL_FOREACH_SAFE(sess->ev_list, ev, ev_tmp)
        {
            if (ev->session_event_cb != NULL && (ev->events & events) && (ev->is_delete == 0))
            {
                ev->session_event_cb(sess, events, pkt, ev->cb_arg);
                DL_DELETE(sess->ev_list, ev);
            }
        }
    }
    return;
}

int stellar_worker_thread_periodic_add(struct stellar *st, stellar_periodic_cb_func *periodic_cb, void *cb_arg, const struct timeval *interval)
{
	// TODO: add to sapp thread timer
	return -1;
}