#include "stun-agent.h"
#include "stun-message.h"
#include "stun-internal.h"
#include "byte-order.h"
#include "sockutil.h"
#include "list.h"
#include <stdlib.h>

struct stun_agent_t* stun_agent_create(struct stun_agent_handler_t* handler, void* param)
{
	struct stun_agent_t* stun;
	stun = (struct stun_agent_t*)calloc(1, sizeof(*stun));
	if (stun)
	{
		LIST_INIT_HEAD(&stun->root);
		memcpy(&stun->handler, handler, sizeof(stun->handler));
		stun->param = param;
	}
	return stun;
}

int stun_agent_destroy(stun_agent_t** pp)
{
	stun_agent_t* stun;
	struct stun_request_t* req;
	struct list_head* pos, *next;

	if (!pp || !*pp)
		return 0;

	stun = *pp;
	list_for_each_safe(pos, next, &stun->root)
	{
		req = list_entry(pos, struct stun_request_t, link);
		free(req);
	}

	free(stun);
	*pp = NULL;
	return 0;
}

// rfc3489 8.2 Shared Secret Requests (p13)
// rfc3489 9.3 Formulating the Binding Request (p17)
int stun_agent_bind(stun_agent_t* stun, stun_request_t* req)
{
	int r;
	struct stun_message_t* msg;
	msg = &req->msg;
	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_BIND);

	r = stun_message_add_credentials(msg, &req->auth);
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_request_send(stun, req) : r;
}

// rfc3489 8.1 Binding Requests (p10)
// rfc3489 9.2 Obtaining a Shared Secret (p15)
int stun_agent_shared_secret(stun_agent_t* stun, stun_request_t* req)
{
	struct stun_message_t* msg;
	msg = &req->msg;

	// This request has no attributes, just the header
	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_SHARED_SECRET);
	return stun_request_send(stun, req);
}

int stun_agent_indication(stun_agent_t* stun, stun_request_t* req)
{
	struct stun_message_t* msg;
	msg = &req->msg;

	// This request has no attributes, just the header
	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_INDICATION, STUN_METHOD_BIND);
	return stun_request_send(stun, req);
}

int stun_agent_send(stun_agent_t* stun, struct stun_message_t* msg, int protocol, const struct sockaddr_storage* local, const struct sockaddr_storage* remote)
{
	int r, bytes;
	uint8_t data[1600];

	assert(stun && msg);
	r = stun_message_write(data, sizeof(data), &msg);
	if (0 != r) return r;

	bytes = msg->header.length + STUN_HEADER_SIZE;
	r = stun->handler.send(stun->param, protocol, local, remote, data, bytes);
	return r == bytes ? 0 : -1;
}

int stun_request_send(stun_agent_t* stun, stun_request_t* req)
{
	int r;
	assert(stun && req);
	stun_request_addref(req);
	list_insert_after(&req->link, &stun->root);
	// TODO: start retransmission timer
	r = stun_agent_send(stun, &req->msg, req->protocol, &req->host, &req->remote);
	if (0 != r)
	{
		stun_request_release(req);
		list_remove(&req->link);
	}
	return r;
}

static struct stun_request_t* stun_agent_find(stun_agent_t* stun, const struct stun_message_t* msg)
{
	struct list_head *ptr, *next;
	struct stun_request_t* entry;
	list_for_each_safe(ptr, next, &stun->root)
	{
		entry = list_entry(ptr, struct stun_request_t, link);
		if (0 == memcmp(entry->msg.header.tid, msg->header.tid, sizeof(msg->header.tid)))
			return entry;
	}
	return NULL;
}

static int stun_agent_onbind(stun_agent_t* stun, const struct stun_request_t* req, struct stun_response_t* resp)
{
	int r;
	r = stun_message_attr_find(&req->msg, STUN_ATTR_RESPONSE_ADDRESS);
	if (-1 != r)
	{
		stun_message_add_address(&resp->msg, STUN_ATTR_REFLECTED_FROM, &resp->remote);
		memcpy(&resp->remote, &req->msg.attrs[r].v.addr, sizeof(resp->remote));
	}

	r = stun_message_attr_find(&req->msg, STUN_ATTR_CHANGE_REQUEST);
	if (-1 != r)
	{
		// TODO change-request
		//memcpy(&resp->remote, &req->msg.attrs[r].v.addr, sizeof(resp->remote));
		//stun_message_add_address(&resp->msg, STUN_ATTR_CHANGED_ADDRESS, &resp->C);
	}

	stun_message_add_address(&resp->msg, STUN_ATTR_SOURCE_ADDRESS, &resp->local);
	stun_message_add_address(&resp->msg, STUN_ATTR_MAPPED_ADDRESS, &req->remote);
	stun_message_add_address(&resp->msg, STUN_ATTR_XOR_MAPPED_ADDRESS, &req->remote);
	return stun->handler.onbind(stun->param, req, resp);
}

int stun_agent_bind_response(const struct stun_response_t* resp, int code)
{
	int r;
	struct stun_message_t* msg;
	msg = &resp->msg;
	if(code < 300)
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_BIND);
	else
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_BIND);

	r = stun_message_add_credentials(msg, &resp->auth);
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_agent_send(resp->stun, resp, resp->protocol, &resp->local, &resp->remote) : r;
}

static int stun_agent_onshared_secret(stun_agent_t* stun, const struct stun_request_t* req, struct stun_response_t* resp)
{
	return stun->handler.onsharedsecret(stun->param, req, resp);
}

// the username MUST be valid for a period of at least 10 minutes.
// It MUST invalidate the username after 30 minutes.
// The password MUST have at least 128 bits
int stun_agent_shared_secret_response(const struct stun_response_t* resp, int code, const char* usr, const char* pwd)
{
	int r;
	struct stun_message_t* msg;
	msg = &resp->msg;

	stun_message_add_error(&resp->msg, 433, "request over TLS");

	if (code < 300)
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_SHARED_SECRET);
	else
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_SHARED_SECRET);
		assert(0x0112 == msg->header.msgtype);
	}

	r = stun_message_add_string(msg, STUN_ATTR_USERNAME, &usr);
	r = stun_message_add_string(msg, STUN_ATTR_PASSWORD, &pwd);
	return 0 == r ? stun_agent_send(resp->stun, resp, resp->protocol, &resp->local, &resp->remote) : r;
}

static int stun_agent_onrequest(stun_agent_t* stun, struct stun_request_t* req)
{
	int r;
	struct stun_response_t* resp;
	resp = stun_response_create(req);
	if (NULL == resp)
		return -1; // -ENOMEM

	// auth
	r = stun->handler.auth(stun->param, req->auth.usr, req->auth.realm, req->auth.nonce, req->auth.pwd);
	stun_message_add_credentials(&req->msg, &req->auth);

	stun_message_add_error(&resp->msg, 401, "requires integrity checks");
	// have MESSAGE-INTEGRITY, don't have username
	stun_message_add_error(&resp->msg, 432, "need username");
	stun_message_add_error(&resp->msg, 430, "the shared secret timeout");
	stun_message_add_error(&resp->msg, 431, "HMAC check error");

	return stun_agent_send(stun);

	switch (STUN_MESSAGE_METHOD(req->msg.header.msgtype))
	{
	case STUN_METHOD_BIND:
		return stun_agent_onbind(stun, req, resp);

	case STUN_METHOD_SHARED_SECRET:
		return stun_agent_onshared_secret(stun, req, resp);

	case STUN_METHOD_ALLOCATE:
		return stun_agent_onallocate(stun, req, resp);

	case STUN_METHOD_REFRESH:
		return stun_agent_onrefresh(stun, req, resp);

	case STUN_METHOD_CREATE_PERMISSION:
		return stun_agent_oncreate_permission(stun, req, resp);

	case STUN_METHOD_CHANNEL_BIND:
		return stun_agent_onchannel_bind(stun, req, resp);

	case STUN_METHOD_DATA:
		return stun_agent_ondata(stun, req, resp);

	default:
		assert(0);
		return -1;
	}
}

static int stun_agent_onindication(stun_agent_t* stun, const struct stun_request_t* req)
{
	switch (STUN_MESSAGE_METHOD(req->msg.header.msgtype))
	{
	case STUN_METHOD_SEND:
		return stun_agent_onsend(stun, req);

	default:
		assert(0);
		return -1;
	}
}

static int stun_agent_onsuccess(stun_agent_t* stun, const struct stun_request_t* req)
{
}

static int stun_agent_onfailure(stun_agent_t* stun, const struct stun_request_t* req)
{
}

int stun_agent_input(stun_agent_t* stun, int protocol, const struct sockaddr_storage* local, const struct sockaddr_storage* remote, const void* data, int bytes)
{
	int i, r;
	struct stun_request_t req;
	struct stun_message_t *msg;

	msg = &req.msg;
	memset(&req, 0, sizeof(req));
	r = stun_message_read(msg, data, bytes);
	if (0 != r)
		return r;

	assert(STUN_PROTOCOL_UDP == protocol || STUN_PROTOCOL_TCP == protocol || STUN_PROTOCOL_TLS == protocol);
	memcpy(&req.reflexive, remote, sizeof(struct sockaddr_storage));
	r = stun_request_setaddr(&req, protocol, remote, local);
	
	for (i = 0; i < msg->nattrs; i++)
	{
		switch (msg->attrs[i].type)
		{
		case STUN_ATTR_NONCE:
			snprintf(req.auth.nonce, sizeof(req.auth.nonce), "%.*s", msg->attrs[i].length, msg->attrs[i].v.string);
			break;

		case STUN_ATTR_REALM:
			snprintf(req.auth.realm, sizeof(req.auth.realm), "%.*s", msg->attrs[i].length, msg->attrs[i].v.string);
			break;

		case STUN_ATTR_USERNAME:
			snprintf(req.auth.usr, sizeof(req.auth.usr), "%.*s", msg->attrs[i].length, msg->attrs[i].v.string);
			break;

		case STUN_ATTR_PASSWORD:
			snprintf(req.auth.pwd, sizeof(req.auth.pwd), "%.*s", msg->attrs[i].length, msg->attrs[i].v.string);
			break;

		case STUN_ATTR_SOURCE_ADDRESS:
			memcpy(&req.host, &msg->attrs[i].v.addr, sizeof(struct sockaddr_storage));
			break;

		case STUN_ATTR_XOR_MAPPED_ADDRESS:
			memcpy(&req.reflexive, &msg->attrs[i].v.addr, sizeof(struct sockaddr_storage));
			break;

		case STUN_ATTR_XOR_RELAYED_ADDRESS:
			memcpy(&req.relay, &msg->attrs[i].v.addr, sizeof(struct sockaddr_storage));
			break;

		case STUN_ATTR_LIFETIME:
			req.lifetime = msg->attrs[i].v.u32;
			break;

		case STUN_ATTR_ERROR_CODE:
			switch (msg->attrs[i].v.errcode.code)
			{
			}
			break;

		default:
			if (msg->attrs[i].type < 0x7fff)
			{
				stun_message_add_error(&resp->msg, 420, "HMAC check error");
				stun_message_add_uint32(&resp->msg, STUN_ATTR_UNKNOWN_ATTRIBUTES, "HMAC check error");
			}

		}
	}

	switch (STUN_MESSAGE_CLASS(req.msg.header.msgtype))
	{
	case STUN_METHOD_CLASS_REQUEST:
		r = stun_agent_onrequest(stun, &req);
		break;

	case STUN_METHOD_CLASS_INDICATION:
		r = stun_agent_onindication(stun, &req);
		break;

	case STUN_METHOD_CLASS_SUCCESS_RESPONSE:
		r = stun_agent_onrequest(stun, resp);
		break;

	case STUN_METHOD_CLASS_FAILURE_RESPONSE:
		r = stun_agent_onrequest(stun, resp);
		break;

	default:
		assert(0);
		r = -1;
	}

	return r;
}
