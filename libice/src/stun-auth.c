#include "stun-internal.h"

static int stun_auth_response(stun_agent_t* stun, struct stun_request_t* req, int code, const char* phrase, const char* realm, const char* nonce)
{
	int r;
	struct stun_message_t msg;
	
	memset(&msg, 0, sizeof(struct stun_message_t));
	memcpy(&msg.header, &req->msg.header, sizeof(struct stun_header_t));
    msg.header.length = 0;
	msg.header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_MESSAGE_METHOD(req->msg.header.msgtype));

	r = stun_message_add_error(&msg, code, phrase);
	if (realm && *realm && nonce && *nonce)
	{
		r = 0 == r ? stun_message_add_string(&msg, STUN_ATTR_REALM, realm) : r;
		r = 0 == r ? stun_message_add_string(&msg, STUN_ATTR_NONCE, nonce) : r;
	}
	
	return 0 == r ? stun_message_send(stun, &msg, req->addr.protocol, &req->addr.host, &req->addr.peer, &req->addr.relay) : r;
}

static int stun_request_rfc3489_auth_check(stun_agent_t* stun, struct stun_request_t* req, const void* data, int bytes)
{
	int r;
	const struct stun_attr_t* username, *integrity;
	username = stun_message_attr_find(&req->msg, STUN_ATTR_USERNAME);
	integrity = stun_message_attr_find(&req->msg, STUN_ATTR_MESSAGE_INTEGRITY);
	
	if (!integrity)
	{
		stun_auth_response(stun, req, 401, "Unauthorized", NULL, NULL);
		return 401;
	}

	if (!username)
	{
		stun_auth_response(stun, req, 432, "Missing Username", NULL, NULL);
		return 432;
	}


	r = stun->handler.auth(stun->param, req->auth.credential, req->auth.usr, NULL, NULL, req->auth.pwd);
	if (0 != r)
	{
		// The Binding Request did contain a MESSAGEINTEGRITY attribute, 
		// but it used a shared secret that has expired. The client should 
		// obtain a new shared secret and try again.
		stun_auth_response(stun, req, 430, "Stale Credentials", NULL, NULL);
		return 430;
	}

	if (0 != stun_message_check_integrity(data, bytes, &req->msg, &req->auth))
	{
		stun_auth_response(stun, req, 431, "Integrity Check Failure", NULL, NULL);
		return 431;
	}

	return 0;
}

// rfc5389 10.1.2. Receiving a Request or Indication(p23)
static int stun_request_rfc5389_short_term_auth_check(stun_agent_t* stun, struct stun_request_t* req, const void* data, int bytes)
{
	int r;
	const struct stun_attr_t* integrity;
	integrity = stun_message_attr_find(&req->msg, STUN_ATTR_MESSAGE_INTEGRITY);

	// If the message does not contain both a MESSAGE-INTEGRITY and a USERNAME attribute
	if (0 == req->auth.usr[0] && !integrity)
	{
		stun_auth_response(stun, req, 400, "Bad Request", NULL, NULL);
		return 400;
	}
	else if (0 == req->auth.usr[0] || !integrity)
	{
		stun_auth_response(stun, req, 401, "Unauthorized", NULL, NULL);
		return 401;
	}

	r = stun->handler.auth(stun->param, req->auth.credential, req->auth.usr, NULL, NULL, req->auth.pwd);
	if (0 != r || 0 != stun_message_check_integrity(data, bytes, &req->msg, &req->auth))
	{
		stun_auth_response(stun, req, 401, "Unauthorized", NULL, NULL);
		return 401;
	}

	return 0;
}

// rfc5389 10.2.2. Receiving a Request(p26)
static int stun_request_rfc5389_long_term_auth_check(stun_agent_t* stun, struct stun_request_t* req, const void* data, int bytes)
{
	int r;
	const struct stun_attr_t* integrity;
	integrity = stun_message_attr_find(&req->msg, STUN_ATTR_MESSAGE_INTEGRITY);

	// If the message does not contain both a MESSAGE-INTEGRITY attribute
	if (!integrity)
	{
		stun->handler.getnonce(stun->param, req->auth.realm, req->auth.nonce);
		stun_auth_response(stun, req, 401, "Unauthorized", req->auth.realm, req->auth.nonce);
		return 401;
	}
	else if (0 == req->auth.usr[0] || 0 == req->auth.realm[0] || 0 == req->auth.nonce[0])
	{
		// If the message contains a MESSAGE-INTEGRITY attribute, but is missing the 
		// USERNAME, REALM, or NONCE attribute, the server MUST generate an error 
		// response with an error code of 400 (Bad Request). This response SHOULD NOT 
		// include a USERNAME, NONCE, REALM, or MESSAGE-INTEGRITY attribute.
		stun_auth_response(stun, req, 400, "Bad Request", NULL, NULL);
		return 400;
	}

	req->auth.credential = STUN_CREDENTIAL_LONG_TERM;
	r = stun->handler.auth(stun->param, req->auth.credential, req->auth.usr, req->auth.realm, req->auth.nonce, req->auth.pwd);
	if (0 != r)
	{
		// If the NONCE is no longer valid, the server MUST generate an error 
		// response with an error code of 438 (Stale Nonce).
		r = stun_auth_response(stun, req, 438, "Stale Nonce", req->auth.realm, req->auth.nonce);
		return 438;
	}

	if (0 != stun_message_check_integrity(data, bytes, &req->msg, &req->auth))
	{
		// If the username in the USERNAME attribute is not valid, the server MUST 
		// generate an error response with an error code of 401 (Unauthorized).

		// If the resulting value does not match the contents of the MESSAGE-INTEGRITY 
		// attribute, the server MUST reject the request with an error response. 
		// This response MUST use an error code of 401 (Unauthorized).
		stun_auth_response(stun, req, 401, "Unauthorized", req->auth.realm, req->auth.nonce);
		return 401;
	}

	return 0;
}

// rfc5389 10.1.3. Receiving a Response (p24)
static int stun_response_rfc5389_short_term_auth_check(stun_agent_t* stun, struct stun_request_t* resp, const void* data, int bytes)
{
    struct stun_request_t* req;
    const struct stun_attr_t* integrity;
    const struct stun_attr_t* fingerprint;
    integrity = stun_message_attr_find(&resp->msg, STUN_ATTR_MESSAGE_INTEGRITY);
    fingerprint = stun_message_attr_find(&resp->msg, STUN_ATTR_FINGERPRINT);
    
    req = stun_agent_find(stun, &resp->msg);
    
    // If the value does not match, or if MESSAGE-INTEGRITY was absent, the response MUST be discarded,
    // as if it was never received.
	if (integrity && (!req || 0 != stun_message_check_integrity(data, bytes, &resp->msg, &req->auth)))
        return -1;
    
	return fingerprint ? stun_message_check_fingerprint(data, bytes, &resp->msg) : 0;
}

// rfc5389 10.2.3. Receiving a Response (p27)
static int stun_response_rfc5389_long_term_auth_check(stun_agent_t* stun, struct stun_request_t* resp, const void* data, int bytes)
{
    struct stun_request_t* req;
    const struct stun_attr_t* integrity;
    const struct stun_attr_t* fingerprint;
    integrity = stun_message_attr_find(&resp->msg, STUN_ATTR_MESSAGE_INTEGRITY);
    fingerprint = stun_message_attr_find(&resp->msg, STUN_ATTR_FINGERPRINT);
    
    req = stun_agent_find(stun, &resp->msg);
    
	// If the value does not match, or if MESSAGE-INTEGRITY was absent, the response MUST be discarded,
	// as if it was never received.
	if (integrity && (!req || 0 != stun_message_check_integrity(data, bytes, &resp->msg, &req->auth)))
		return -1;

    return fingerprint ? stun_message_check_fingerprint(data, bytes, &resp->msg) : 0;
}

int stun_agent_auth(stun_agent_t* stun, struct stun_request_t* req, const void* data, int bytes)
{
	switch (STUN_MESSAGE_CLASS(req->msg.header.msgtype))
	{
	case STUN_METHOD_CLASS_REQUEST:
	case STUN_METHOD_CLASS_INDICATION:
        if(STUN_METHOD_DATA == STUN_MESSAGE_METHOD(req->msg.header.msgtype) || STUN_METHOD_BIND == STUN_MESSAGE_METHOD(req->msg.header.msgtype))
           return 0;
           
		if (STUN_RFC_3489 == stun->rfc)
			return stun_request_rfc3489_auth_check(stun, req, data, bytes);
		else
			return 0 == stun->auth_term ? stun_request_rfc5389_short_term_auth_check(stun, req, data, bytes) : stun_request_rfc5389_long_term_auth_check(stun, req, data, bytes);

	case STUN_METHOD_CLASS_SUCCESS_RESPONSE:
		return (STUN_RFC_3489 == stun->rfc || 0 == stun->auth_term) ? stun_response_rfc5389_short_term_auth_check(stun, req, data, bytes) : stun_response_rfc5389_long_term_auth_check(stun, req, data, bytes);

	case STUN_METHOD_CLASS_FAILURE_RESPONSE:
		return 0; // don't need check

	default:
		assert(0);
		return -1;
	}
}
