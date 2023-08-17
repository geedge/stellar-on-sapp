#include "stellar/session.h"
#include "stellar/stellar.h"
#include "stellar/utils.h"
#include "stellar/session_exdata.h"

#include "stellar_internal.h"
#include "adapter.h"
#include "session_manager.h"

#include "uthash/utarray.h"

#include <MESA/stream.h>


inline void adapter_session_poll(struct session *sess)
{
	session_defer_loop(sess);
}



struct streaminfo;
//streaminfo open
struct session *adapter_session_open(struct stellar *st, struct streaminfo *stream, void *a_packet)
{
	enum session_type type;
	if(stream->type==STREAM_TYPE_TCP)
	{
		type=SESSION_TYPE_TCP;
	}
	else if(stream->type==STREAM_TYPE_UDP)
	{
		type=SESSION_TYPE_UDP;
	}
	else
	{
		return NULL;
	}
	struct session *sess=session_new(st, stream, type, stream->threadnum);
	session_dispatch(sess, SESSION_STATE_OPENING, a_packet);
	return sess;
}

void adapter_session_active(struct streaminfo *stream, struct session *sess, void *a_packet)
{
	if (sess)
	{
		session_dispatch( sess, SESSION_STATE_ACTIVE, a_packet);
	}
	return;
}

//streaminfo close, or firewall active close streaminfo  
void adapter_session_close(struct streaminfo *stream, struct session *sess, void *a_packet)
{
	if(sess)
	{
		session_dispatch(sess, SESSION_STATE_CLOSING, a_packet);
		session_free(sess);
	}
	return;
}

const char* session_get0_readable_addr(struct session *sess)
{
	return printaddr(&sess->stream->addr, sess->stream->threadnum);
}

const char *session_get0_current_payload(struct session *sess, size_t *payload_len)
{
	*payload_len = (size_t)sess->stream->ptcpdetail->datalen;
	return (const char*)sess->stream->ptcpdetail->pdata;
}

const struct packet *session_get0_current_packet(struct session *sess)
{
	return (const struct packet *)get_rawpkt_from_streaminfo(sess->stream);
}

int packet_get_direction(const struct packet *pkt)
{
	// TODO: 
	return -1;
}   	

const char *packet_get0_data(const struct packet *pkt, size_t *data_len)
{
	// TODO:
	return NULL;
}

int  session_get_direction(const struct session *sess)
{
	// TODO: check route dir
	if(sess->stream)
	{
		if(sess->stream->curdir==DIR_C2S)
		{
			return SESSION_DIRECTION_IN;
		}
		else if(sess->stream->curdir==DIR_S2C)
		{
			return SESSION_DIRECTION_OUT;
		}
	}
	return -1;
} 

int session_is_symmetric(const struct session *sess, unsigned char *flag)
{
	int is_symmetric=0;
	if(sess->stream)
	{
		if(sess->stream->curdir==DIR_DOUBLE)
		{
			*flag=(SESSION_SEEN_C2S_FLOW|SESSION_SEEN_S2C_FLOW);
			is_symmetric=1;
		}
		else if(sess->stream->curdir==DIR_C2S)
		{
			*flag=SESSION_SEEN_C2S_FLOW;
		}
		else if(sess->stream->curdir==DIR_S2C)
		{
			*flag=SESSION_SEEN_S2C_FLOW;
		}
	}
	return is_symmetric;
}	

int stellar_get_worker_thread_num(struct stellar *st)
{
	return get_thread_count();
}

int stellar_get_current_thread_id(struct stellar *st)
{
	// TODO:
	return 0;
}