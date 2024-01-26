/*
**********************************************************************************************
*	File: http_decoder_inc.h
*   Description: 
*	Authors: Liu WenTan <liuwentan@geedgenetworks.com>
*	Date:    2024-01-10
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

#ifndef _HTTP_DECODER_INC_H_
#define _HTTP_DECODER_INC_H_

#ifdef __cplusplus
extern "C"
{
#endif

struct http_message;
struct http_message *http_message_new(enum http_message_type type, void *data);

#ifdef __cplusplus
}
#endif

#endif