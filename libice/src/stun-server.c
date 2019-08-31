#include "stun-internal.h"
#include "stun-message.h"

int stun_server_onbind(struct stun_agent_t* stun, const struct stun_request_t* req, struct stun_response_t* resp)
{
	const struct stun_attr_t* attr;
	attr = stun_message_attr_find(&req->msg, STUN_ATTR_RESPONSE_ADDRESS);
	if (attr)
	{
		stun_message_add_address(&resp->msg, STUN_ATTR_REFLECTED_FROM, (const struct sockaddr*)&resp->addr.peer);
		memcpy(&resp->addr.peer, &attr->v.addr, sizeof(resp->addr.peer));
	}

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_CHANGE_REQUEST);
	if (attr)
	{
		// TODO change-request
		//memcpy(&resp->remote, &attr->v.addr, sizeof(resp->remote));
		//stun_message_add_address(&resp->msg, STUN_ATTR_CHANGED_ADDRESS, &resp->C);
	}
    
    // rfc5780 7.5. RESPONSE-PORT
    attr = stun_message_attr_find(&req->msg, STUN_ATTR_RESPONSE_PORT);
    if(attr && (attr->v.u32 >> 16) > 0)
    {
        socket_addr_setport((struct sockaddr*)&resp->addr.peer, socket_addr_len((const struct sockaddr*)&resp->addr.peer), (u_short)(attr->v.u32 >> 16));
    }

	stun_message_add_address(&resp->msg, STUN_ATTR_SOURCE_ADDRESS, (const struct sockaddr*)&resp->addr.host);
	stun_message_add_address(&resp->msg, STUN_ATTR_MAPPED_ADDRESS, (const struct sockaddr*)&resp->addr.peer);
	stun_message_add_address(&resp->msg, STUN_ATTR_XOR_MAPPED_ADDRESS, (const struct sockaddr*)&resp->addr.peer);
    stun_message_add_address(&resp->msg, STUN_ATTR_RESPONSE_ORIGIN, (const struct sockaddr*)&resp->addr.host);
	return stun->handler.onbind(stun->param, resp, req);
}

int stun_agent_bind_response(struct stun_response_t* resp, int code, const char* pharse)
{
	struct stun_message_t* msg;
	msg = &resp->msg;

	if (code < 300)
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_BIND);
		assert(msg->header.msgtype == 0x0101);
        stun_message_add_credentials(msg, &resp->auth);
		stun_message_add_fingerprint(msg);
	}
	else
	{
		msg->nattrs = 0; // reset attributes
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_BIND);
		assert(msg->header.msgtype == 0x0111);
        stun_message_add_error(&resp->msg, code, pharse);
	}

	return stun_response_send(resp->stun, resp);
}

int stun_server_onshared_secret(struct stun_agent_t* stun, const struct stun_request_t* req, struct stun_response_t* resp)
{
	return stun->handler.onsharedsecret ? stun->handler.onsharedsecret(stun->param, resp, req) : 0;
}

// the username MUST be valid for a period of at least 10 minutes.
// It MUST invalidate the username after 30 minutes.
// The password MUST have at least 128 bits
int stun_agent_shared_secret_response(struct stun_response_t* resp, int code, const char* pharse, const char* usr, const char* pwd)
{
	struct stun_message_t* msg;
	msg = &resp->msg;

	if ((!usr || !*usr || !pwd || !*pwd) && code < 300)
	{
		stun_response_destroy(&resp);
		return -1; // invalid code
	}

	if (code < 300)
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_SHARED_SECRET);
		assert(msg->header.msgtype == 0x0102);
		stun_message_add_string(msg, STUN_ATTR_USERNAME, usr);
		stun_message_add_string(msg, STUN_ATTR_PASSWORD, pwd);
	}
	else
	{
		msg->nattrs = 0; // reset attributes
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_SHARED_SECRET);
		assert(msg->header.msgtype == 0x0112);
		assert(0x0112 == msg->header.msgtype);

		// TODO:
		//stun_message_add_error(&resp->msg, 433, "request over TLS");
		stun_message_add_error(&resp->msg, code, pharse);
	}

	return stun_response_send(resp->stun, resp);
}

int stun_server_onbindindication(struct stun_agent_t* stun, const struct stun_request_t* req)
{
    return stun->handler.onbindindication ? stun->handler.onbindindication(stun->param, req) : 0;
}
