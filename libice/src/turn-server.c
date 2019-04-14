#include "stun-agent.h"
#include "stun-internal.h"
#include "turn-internal.h"
#include "sys/system.h"

// rfc5766 6.2. Receiving an Allocate Request (p24)
int turn_agent_onallocate(const struct stun_transaction_t* req, struct stun_transaction_t* resp)
{
	int even_port;
	uint32_t lifetime;
	const struct stun_attr_t* attr;
	struct turn_allocation_t* allocate;
	attr = stun_message_attr_find(&req->msg, STUN_ATTR_REQUESTED_TRANSPORT);
	allocate->peertransport = attr ? attr->v.u32 : STUN_PROTOCOL_UDP;

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_LIFETIME);
	lifetime = attr ? attr->v.u32 : TURN_LIFETIME;
	lifetime = MAX(lifetime, TURN_LIFETIME);
	lifetime = MIN(lifetime, 3600); // 1-hour
	allocate->expire = system_clock() + lifetime * 1000;

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_DONT_FRAGMENT);
	allocate->dontfragment = attr ? attr->v.u32 : 1;

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_EVEN_PORT);
	even_port = attr ? attr->v.u32 : 0;

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_RESERVATION_TOKEN);
	if (attr)
	{
		even_port = attr ? attr->v.u32 : 0;
		stun_message_add_uint32(&resp->msg, STUN_ATTR_RESERVATION_TOKEN, lifetime);
	}

	// 1. long-term credential auth

	// 2. checks the 5-tuple
	stun_message_add_error(&resp->msg, 437, "Allocation Mismatch");

	// 5/6.
	stun_message_add_error(&resp->msg, 508, "Insufficient Capacity");

	// 7.
	stun_message_add_error(&resp->msg, 486, "Allocation Quota Reached");

	memcpy(&allocate->client, &req->relay, sizeof(allocate->client));
	memcpy(&allocate->server, &req->remote, sizeof(allocate->server));

	// In all cases, the server SHOULD only allocate ports from the range
	// 49152 - 65535 (the Dynamic and/or Private Port range [Port-Numbers]),

	// alloc relayed transport address
	memcpy(&allocate->relay, &req->remote, sizeof(allocate->relay));
	
	
	// reply
	stun_message_add_uint32(&resp->msg, STUN_ATTR_LIFETIME, lifetime);
	stun_message_add_address(&resp->msg, STUN_ATTR_XOR_MAPPED_ADDRESS, &allocate->client);
	stun_message_add_address(&resp->msg, STUN_ATTR_XOR_RELAYED_ADDRESS, &allocate->relay);
	return req->stun->handler.onbind(req->stun->param, req, resp);
}

int turn_agent_allocate_response(const struct stun_transaction_t* resp, int code)
{
	int r;
	struct stun_message_t* msg;
	msg = &resp->msg;
	if (code < 300)
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_ALLOCATE);
	else
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_ALLOCATE);

	r = stun_message_add_credentials(msg, &resp->auth);
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_message_send(resp->stun, &resp->msg, resp->protocol, &resp->host, &resp->remote) : r;
}

int turn_agent_onrefresh(const struct stun_transaction_t* req, struct stun_transaction_t* resp)
{
	int r;
	uint32_t lifetime;
	const struct stun_attr_t* attr;
	struct turn_allocation_t* allocate;

	allocate = turn_allocate_find(req);

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_LIFETIME);
	lifetime = attr ? attr->v.u32 : 0;

	if (0 == lifetime)
	{
		turn_allocation_destroy(&allocate);
	}
	else
	{
		lifetime = MAX(lifetime, TURN_LIFETIME);
		lifetime = MIN(lifetime, 3600); // 1-hour
		allocate->expire = system_clock() + lifetime * 1000;
	}

	// reply
	stun_message_add_uint32(&resp->msg, STUN_ATTR_LIFETIME, lifetime);
	return req->stun->handler.onrefresh(req->stun->param, req, resp);
}

int turn_agent_refresh_response(const struct stun_transaction_t* resp, int code)
{
	int r;
	struct stun_message_t* msg;
	msg = &resp->msg;
	if (code < 300)
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_REFRESH);
	else
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_REFRESH);

	r = stun_message_add_credentials(msg, &resp->auth);
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_message_send(resp->stun, &resp->msg, resp->protocol, &resp->host, &resp->remote) : r;
}

static int turn_agent_add_permission(void* param, const struct stun_attr_t* attr)
{
	struct turn_allocation_t* allocate;
	allocate = (struct turn_allocation_t*)param;
	return turn_allocation_add_permission(allocate, &attr->v.addr);
}

int turn_agent_oncreate_permission(const struct stun_transaction_t* req, struct stun_transaction_t* resp)
{
	int r;
	uint32_t lifetime;
	const struct stun_attr_t* attr;
	struct turn_allocation_t* allocate;

	allocate = turn_allocate_find(req);

	r = stun_message_attr_list(&req->msg, STUN_ATTR_XOR_PEER_ADDRESS, turn_agent_add_permission, allocate);
	
	stun_message_add_error(&resp->msg, 400, "Bad Request");

	stun_message_add_error(&resp->msg, 508, "Insufficient Capacity)");

	// restrictions on the IP address allowed in the XOR-PEER-ADDRESS attribute
	stun_message_add_error(&resp->msg, 403, "Forbidden)");

	// reply
	return req->stun->handler.onpermission(req->stun->param, req, resp);
}

int turn_agent_permission_response(const struct stun_transaction_t* resp, int code)
{
	int r;
	struct stun_message_t* msg;
	msg = &resp->msg;
	if (code < 300)
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_CREATE_PERMISSION);
	else
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_CREATE_PERMISSION);

	r = stun_message_add_credentials(msg, &resp->auth);
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_message_send(resp->stun, &resp->msg, resp->protocol, &resp->host, &resp->remote) : r;
}

int turn_agent_onchannel_bind(const struct stun_transaction_t* req, struct stun_transaction_t* resp)
{
	int r;
	uint32_t lifetime;
	const struct stun_attr_t* peer;
	const struct stun_attr_t* channel;
	struct turn_allocation_t* allocate;

	allocate = turn_allocate_find(req);

	peer = stun_message_attr_find(&req->msg, STUN_ATTR_XOR_PEER_ADDRESS);
	channel = stun_message_attr_find(&req->msg, STUN_ATTR_CHANNEL_NUMBER);
	if (!peer || !channel || channel->v.u16 < 0x4000 || channel->v.u16 > 0x7FFE)
	{
		stun_message_add_error(&resp->msg, 400, "Bad Request");
	}

	stun_message_add_error(&resp->msg, 400, "Bad Request");

	stun_message_add_error(&resp->msg, 508, "Insufficient Capacity)");

	// restrictions on the IP address allowed in the XOR-PEER-ADDRESS attribute
	stun_message_add_error(&resp->msg, 403, "Forbidden)");

	// A ChannelBind transaction also creates or refreshes a permission towards the peer
	r = turn_allocation_add_permission(allocate, &peer->v.addr);
	r = 0 == r ? turn_allocation_add_channel(allocate, &peer->v.addr, channel->v.u16) : r;
	if (0 != r)
	{
		stun_message_add_error(&resp->msg, 400, "Bad Request");
	}

	// reply
	return req->stun->handler.onpermission(req->stun->param, req, resp);
}

int turn_agent_channel_response(const struct stun_transaction_t* resp, int code)
{
	int r;
	struct stun_message_t* msg;
	msg = &resp->msg;
	if (code < 300)
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_CHANNEL_BIND);
	else
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_CHANNEL_BIND);

	r = stun_message_add_credentials(msg, &resp->auth);
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_message_send(resp->stun, &resp->msg, resp->protocol, &resp->host, &resp->remote) : r;
}

int stun_agent_onsend(const struct stun_transaction_t* req, struct stun_transaction_t* resp)
{
	int r;
	int fragment;
	uint32_t lifetime;
	struct sockaddr_storage peer;
	const struct stun_attr_t* attr;
	struct turn_allocation_t* allocate;

	allocate = turn_allocate_find(req);

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_DONT_FRAGMENT);
	fragment = attr ? attr->v.u8 : 0;

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_XOR_PEER_ADDRESS);
	if (!attr) return 0; // discard
	memcpy(&peer, &attr->v.addr, sizeof(struct sockaddr_storage));

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_DATA);
	if (!attr) return 0; // discard

	return req->stun->handler.send(req->stun->param, allocate->peertransport, &allocate->relay, &peer, attr->v.ptr, attr->length);
}
