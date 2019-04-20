#include "stun-message.h"
#include "stun-internal.h"

int stun_agent_onbind(const struct stun_request_t* req, struct stun_response_t* resp)
{
	const struct stun_attr_t* attr;
	attr = stun_message_attr_find(&req->msg, STUN_ATTR_RESPONSE_ADDRESS);
	if (attr)
	{
		stun_message_add_address(&resp->msg, STUN_ATTR_REFLECTED_FROM, &resp->addr.peer);
		memcpy(&resp->addr.peer, &attr->v.addr, sizeof(resp->addr.peer));
	}

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_CHANGE_REQUEST);
	if (attr)
	{
		// TODO change-request
		//memcpy(&resp->remote, &attr->v.addr, sizeof(resp->remote));
		//stun_message_add_address(&resp->msg, STUN_ATTR_CHANGED_ADDRESS, &resp->C);
	}

	stun_message_add_address(&resp->msg, STUN_ATTR_SOURCE_ADDRESS, &resp->addr.host);
	stun_message_add_address(&resp->msg, STUN_ATTR_MAPPED_ADDRESS, &resp->addr.peer);
	stun_message_add_address(&resp->msg, STUN_ATTR_XOR_MAPPED_ADDRESS, &resp->addr.peer);
	return req->stun->handler.onbind(req->stun->param, resp, req);
}

int stun_agent_bind_response(struct stun_response_t* resp, int code, const char* pharse)
{
	int r;
	struct stun_message_t* msg;
	msg = &resp->msg;

	r = 0;
	if (code < 300)
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_BIND);
		r = 0 == r ? stun_message_add_credentials(msg, &resp->auth) : r;
		r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	}
	else
	{
		msg->nattrs = 0; // reset attributes
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_BIND);
		r = 0 == r ? stun_message_add_error(&resp->msg, code, pharse) : r;
	}

	return 0 == r ? stun_response_send(resp->stun, resp) : r;
}

int stun_agent_onshared_secret(const struct stun_request_t* req, struct stun_response_t* resp)
{
	return req->stun->handler.onsharedsecret(req->stun->param, resp, req);
}

// the username MUST be valid for a period of at least 10 minutes.
// It MUST invalidate the username after 30 minutes.
// The password MUST have at least 128 bits
int stun_agent_shared_secret_response(struct stun_response_t* resp, int code, const char* pharse, const char* usr, const char* pwd)
{
	int r;
	struct stun_message_t* msg;
	msg = &resp->msg;

	if ((!usr || !*usr || !pwd || !*pwd) && code < 300)
	{
		stun_response_destroy(&resp);
		return -1; // invalid code
	}

	r = 0;
	if (code < 300)
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_SHARED_SECRET);
		r = 0 == r ? stun_message_add_string(msg, STUN_ATTR_USERNAME, usr) : r;
		r = 0 == r ? stun_message_add_string(msg, STUN_ATTR_PASSWORD, pwd) : r;
	}
	else
	{
		msg->nattrs = 0; // reset attributes
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_SHARED_SECRET);
		assert(0x0112 == msg->header.msgtype);

		// TODO:
		//r = 0 == r ? stun_message_add_error(&resp->msg, 433, "request over TLS") : r;
		r = 0 == r ? stun_message_add_error(&resp->msg, code, pharse) : r;
	}

	return 0 == r ? stun_response_send(resp->stun, resp) : r;
}
