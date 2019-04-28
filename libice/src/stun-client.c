#include "stun-message.h"
#include "stun-internal.h"
#include "byte-order.h"
#include "sockutil.h"
#include "list.h"
#include <stdlib.h>

// rfc3489 8.2 Shared Secret Requests (p13)
// rfc3489 9.3 Formulating the Binding Request (p17)
int stun_agent_bind(stun_request_t* req)
{
	struct stun_message_t* msg;
	msg = &req->msg;
	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_BIND);
	assert(msg->header.msgtype == 0x0001);

    // backwards compatibility with rfc 3489
    stun_message_add_credentials(msg, &req->auth);
	stun_message_add_fingerprint(msg);
	return stun_request_send(req->stun, req);
}

// rfc3489 8.1 Binding Requests (p10)
// rfc3489 9.2 Obtaining a Shared Secret (p15)
int stun_agent_shared_secret(stun_request_t* req)
{
	struct stun_message_t* msg;
	msg = &req->msg;

	// This request has no attributes, just the header
	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_SHARED_SECRET);
	assert(msg->header.msgtype == 0x0002);
	return stun_request_send(req->stun, req);
}
