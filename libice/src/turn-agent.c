#include "stun-agent.h"
#include "stun-internal.h"

// rfc3489 8.2 Shared Secret Requests (p13)
// rfc3489 9.3 Formulating the Binding Request (p17)
int turn_agent_allocate(stun_agent_t* stun, stun_request_t* req)
{
	int r;
	struct stun_message_t* msg;
	msg = &req->msg;
	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_ALLOCATE);
	
	stun_message_add_uint32(msg, STUN_ATTR_REQUESTED_TRANSPORT, TURN_TRANSPORT_UDP << 24);
	stun_message_add_uint32(msg, STUN_ATTR_LIFETIME, TURN_LIFETIME);
//	stun_message_add_flag(msg, STUN_ATTR_DONT_FRAGMENT);
//	stun_message_add_uint8(msg, STUN_ATTR_EVEN_PORT, 0x80);

	r = stun_message_add_credentials(msg, &req->auth);
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_request_send(stun, req) : r;
}

int turn_agent_refresh(stun_agent_t* stun, stun_request_t* req, int expired)
{
	int r;
	struct stun_message_t* msg;
	msg = &req->msg;
	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_REFRESH);
	
	stun_message_add_uint32(msg, STUN_ATTR_LIFETIME, expired);

	r = stun_message_add_credentials(msg, &req->auth);
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_request_send(stun, req) : r;
}

int turn_agent_permission(stun_agent_t* stun, stun_request_t* req, const struct sockaddr_storage* peer)
{
	int r;
	struct stun_message_t* msg;
	msg = &req->msg;

	if (AF_INET != peer->ss_family && AF_INET6 != peer->ss_family)
		return -1;

	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_REFRESH);
	
	stun_message_add_address(msg, STUN_ATTR_XOR_PEER_ADDRESS, peer);

	r = stun_message_add_credentials(msg, &req->auth);
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_request_send(stun, req) : r;
}

int turn_agent_channel(stun_agent_t* stun, stun_request_t* req, const struct sockaddr_storage* peer, uint16_t channel)
{
	int r;
	struct stun_message_t* msg;
	msg = &req->msg;

	if (AF_INET != peer->ss_family && AF_INET6 != peer->ss_family)
		return -1;

	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_CHANNEL_BIND);
	
	stun_message_add_address(msg, STUN_ATTR_XOR_PEER_ADDRESS, peer);
	stun_message_add_uint32(msg, STUN_ATTR_CHANNEL_NUMBER, channel << 16);

	r = stun_message_add_credentials(msg, &req->auth);
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_request_send(stun, req) : r;
}

int turn_agent_send(stun_agent_t* stun, stun_request_t* req, const struct sockaddr_storage* peer, const void* data, int bytes)
{
	int r;
	struct stun_message_t* msg;
	msg = &req->msg;
	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_INDICATION, STUN_METHOD_SEND);
	
	stun_message_add_flag(msg, STUN_ATTR_DONT_FRAGMENT);
	stun_message_add_address(msg, STUN_ATTR_XOR_PEER_ADDRESS, peer);
	
	msg->attrs[msg->nattrs].type = STUN_ATTR_DATA;
	msg->attrs[msg->nattrs].length = 4;
	msg->attrs[msg->nattrs].v.u64 = 0;
	msg->header.length += 4 + ALGIN_4BYTES(msg->attrs[msg->nattrs++].length);

	r = stun_message_add_credentials(msg, &req->auth);
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_request_send(stun, req) : r;
}
