#include "stun-agent.h"
#include "stun-message.h"
#include "stun-internal.h"
#include "turn-internal.h"
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
	struct stun_transaction_t* req;
	struct list_head* pos, *next;

	if (!pp || !*pp)
		return 0;

	stun = *pp;
	list_for_each_safe(pos, next, &stun->root)
	{
		req = list_entry(pos, struct stun_transaction_t, link);
		free(req);
	}

	free(stun);
	*pp = NULL;
	return 0;
}

int stun_message_send(stun_agent_t* stun, struct stun_message_t* msg, int protocol, const struct sockaddr_storage* local, const struct sockaddr_storage* remote)
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

int stun_transaction_send(stun_agent_t* stun, stun_transaction_t* t)
{
	int r;
	assert(stun && t);
	stun_transaction_addref(t);
	list_insert_after(&t->link, &stun->root);
	// TODO: start retransmission timer
	r = stun_message_send(stun, &t->msg, t->protocol, &t->host, &t->remote);
	if (0 != r)
	{
		stun_transaction_release(t);
		list_remove(&t->link);
	}
	return r;
}

static int stun_agent_onrequest(stun_agent_t* stun, struct stun_transaction_t* req)
{
	int r;
	struct stun_transaction_t* resp;
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
		return stun_agent_onbind(req, resp);

	case STUN_METHOD_SHARED_SECRET:
		return stun_agent_onshared_secret(req, resp);

	case STUN_METHOD_ALLOCATE:
		return turn_agent_onallocate(req, resp);

	case STUN_METHOD_REFRESH:
		return turn_agent_onrefresh(req, resp);

	case STUN_METHOD_CREATE_PERMISSION:
		return turn_agent_oncreate_permission(req, resp);

	case STUN_METHOD_CHANNEL_BIND:
		return turn_agent_onchannel_bind(req, resp);

	case STUN_METHOD_DATA:
		return turn_agent_ondata(req, resp);

	default:
		assert(0);
		return -1;
	}
}

static int stun_agent_onindication(stun_agent_t* stun, const struct stun_transaction_t* req)
{
	switch (STUN_MESSAGE_METHOD(req->msg.header.msgtype))
	{
	case STUN_METHOD_SEND: // client -> turn server
		return stun_agent_onsend(stun, req);

	case STUN_METHOD_DATA: // turn server -> client
		return stun_agent_ondata(stun, req);

	default:
		assert(0);
		return -1;
	}
}

static struct stun_transaction_t* stun_agent_find(stun_agent_t* stun, const struct stun_message_t* msg)
{
	struct list_head *ptr, *next;
	struct stun_transaction_t* entry;
	list_for_each_safe(ptr, next, &stun->root)
	{
		entry = list_entry(ptr, struct stun_transaction_t, link);
		if (0 == memcmp(entry->msg.header.tid, msg->header.tid, sizeof(msg->header.tid)))
			return entry;
	}
	return NULL;
}

static int stun_agent_onsuccess(stun_agent_t* stun, const struct stun_transaction_t* resp)
{
	int r;
	struct stun_transaction_t* req;
	req = stun_agent_find(stun, &resp->msg);
	if (!req)
		return 0; // discard

	// TODO: stop timer

	r = req->handler(req->param, resp, 0, "OK");

	stun_agent_remove(&req->link);
	return r;
}

static int stun_agent_onfailure(stun_agent_t* stun, const struct stun_transaction_t* resp, int code, const char* phrase)
{
	int r;
	struct stun_transaction_t* req;
	req = stun_agent_find(stun, &resp->msg);
	if (!req)
		return 0; // discard

	// TODO: stop timer

	r = stun_message_attr_find(&resp->msg, STUN_ATTR_ERROR_CODE);
	if (-1 != r)
		r = req->handler(req->param, resp, resp->msg.attrs[r].v.errcode.code, resp->msg.attrs[r].v.errcode.reason_phrase);
	else
		r = req->handler(req->param, resp, 500, "Server internal error");

	stun_agent_remove(&req->link);
	return r;
}

int stun_agent_input(stun_agent_t* stun, int protocol, const struct sockaddr_storage* local, const struct sockaddr_storage* remote, const void* data, int bytes)
{
	int i, r;
	uint16_t method;
	struct stun_transaction_t req;
	struct stun_message_t *msg;

	if (stun_agent_is_relay_addr(local))
	{
		// TODO: TURN data
		// check permission
		// check channel bind
		return 0;
	}

	method = STUN_MESSAGE_METHOD(req->msg.header.msgtype);
	if (method)
	{
	}

	msg = &req.msg;
	req.stun = stun;
	memset(&req, 0, sizeof(req));
	r = stun_message_read(msg, data, bytes);
	if (0 != r)
		return r;

	assert(STUN_PROTOCOL_UDP == protocol || STUN_PROTOCOL_TCP == protocol || STUN_PROTOCOL_TLS == protocol);
	memcpy(&req.reflexive, remote, sizeof(struct sockaddr_storage));
	r = stun_transaction_setaddr(&req, protocol, remote, local);

	for (i = 0; i < msg->nattrs; i++)
	{
		switch (msg->attrs[i].type)
		{
		case STUN_ATTR_NONCE:
			snprintf(req.auth.nonce, sizeof(req.auth.nonce), "%.*s", msg->attrs[i].length, msg->attrs[i].v.ptr);
			break;

		case STUN_ATTR_REALM:
			snprintf(req.auth.realm, sizeof(req.auth.realm), "%.*s", msg->attrs[i].length, msg->attrs[i].v.ptr);
			break;

		case STUN_ATTR_USERNAME:
			snprintf(req.auth.usr, sizeof(req.auth.usr), "%.*s", msg->attrs[i].length, msg->attrs[i].v.ptr);
			break;

		case STUN_ATTR_PASSWORD:
			snprintf(req.auth.pwd, sizeof(req.auth.pwd), "%.*s", msg->attrs[i].length, msg->attrs[i].v.ptr);
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
		r = stun_agent_onsuccess(stun, &req);
		break;

	case STUN_METHOD_CLASS_FAILURE_RESPONSE:
		r = stun_agent_onfailure(stun, &req);
		break;

	default:
		assert(0);
		r = -1;
	}

	return r;
}
