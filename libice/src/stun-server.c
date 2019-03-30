#include "stun-message.h"
#include "stun-internal.h"

int stun_agent_onbind(const struct stun_transaction_t* req, struct stun_transaction_t* resp)
{
	const struct stun_attr_t* attr;
	attr = stun_message_attr_find(&req->msg, STUN_ATTR_RESPONSE_ADDRESS);
	if (attr)
	{
		stun_message_add_address(&resp->msg, STUN_ATTR_REFLECTED_FROM, &resp->remote);
		memcpy(&resp->remote, &attr->v.addr, sizeof(resp->remote));
	}

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_CHANGE_REQUEST);
	if (attr)
	{
		// TODO change-request
		//memcpy(&resp->remote, &attr->v.addr, sizeof(resp->remote));
		//stun_message_add_address(&resp->msg, STUN_ATTR_CHANGED_ADDRESS, &resp->C);
	}

	stun_message_add_address(&resp->msg, STUN_ATTR_SOURCE_ADDRESS, &resp->host);
	stun_message_add_address(&resp->msg, STUN_ATTR_MAPPED_ADDRESS, &req->remote);
	stun_message_add_address(&resp->msg, STUN_ATTR_XOR_MAPPED_ADDRESS, &req->remote);
	return req->stun->handler.onbind(req->stun->param, req, resp);
}

int stun_agent_bind_response(const struct stun_transaction_t* resp, int code)
{
	int r;
	struct stun_message_t* msg;
	msg = &resp->msg;
	if (code < 300)
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_BIND);
	else
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_BIND);

	r = stun_message_add_credentials(msg, &resp->auth);
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_message_send(resp->stun, &resp->msg, resp->protocol, &resp->host, &resp->remote) : r;
}

int stun_agent_onshared_secret(const struct stun_transaction_t* req, struct stun_transaction_t* resp)
{
	return req->stun->handler.onsharedsecret(req->stun->param, req, resp);
}

// the username MUST be valid for a period of at least 10 minutes.
// It MUST invalidate the username after 30 minutes.
// The password MUST have at least 128 bits
int stun_agent_shared_secret_response(const struct stun_transaction_t* resp, int code, const char* usr, const char* pwd)
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
	return 0 == r ? stun_message_send(resp->stun, &resp->msg, resp->protocol, &resp->host, &resp->remote) : r;
}
