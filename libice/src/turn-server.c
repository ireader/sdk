#include "stun-agent.h"
#include "stun-internal.h"
#include "turn-internal.h"
#include "sys/system.h"

static int stun_server_response_failure(struct stun_response_t* resp, int code, const char* phrase)
{
	int r;
	struct stun_message_t *msg;

	msg = &resp->msg;
	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_MESSAGE_METHOD(msg->header.msgtype));

	r = stun_message_add_error(msg, code, phrase);
	return 0 == r ? stun_response_send(resp->stun, resp) : r;
}

// rfc5766 6.2. Receiving an Allocate Request (p24)
int turn_agent_onallocate(const struct stun_request_t* req, struct stun_response_t* resp)
{
	int r = 0;
	int even_port;
	uint32_t lifetime;
	const struct stun_attr_t* attr;
	struct turn_allocation_t* allocate;

	// 1. long-term credential auth

	// 2. checks the 5-tuple
	allocate = turn_agent_allocation_find_by_address(&resp->stun->turnservers, &req->addr.host, &req->addr.peer);
	if (NULL != allocate && allocate->expire < system_clock())
		return stun_server_response_failure(resp, 437, "Allocation Mismatch");

	allocate = turn_allocation_create();
	if (!allocate)
		return stun_server_response_failure(resp, 486, "Allocation Quota Reached");
	turn_agent_allocation_insert(resp->stun, allocate);
	
	// 3. check REQUESTED-TRANSPORT
	if (STUN_PROTOCOL_UDP != allocate->peertransport)
		return stun_server_response_failure(resp, 442, "Unsupported Transport Protocol");

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_REQUESTED_TRANSPORT);
	allocate->peertransport = attr ? attr->v.u32 : STUN_PROTOCOL_UDP;

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_LIFETIME);
	lifetime = attr ? attr->v.u32 : TURN_LIFETIME;
	lifetime = lifetime > TURN_LIFETIME ? lifetime : TURN_LIFETIME;
	lifetime = lifetime < 3600 ? lifetime : 3600; // 1-hour
	allocate->expire = lifetime * 1000;

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_DONT_FRAGMENT);
	allocate->dontfragment = attr ? attr->v.u32 : 1;

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_EVEN_PORT);
	even_port = attr ? attr->v.u32 : 0;

	// 5. check RESERVATION-TOKEN
	attr = stun_message_attr_find(&req->msg, STUN_ATTR_RESERVATION_TOKEN);
	if (attr)
	{
		if (even_port != 0)
			return stun_server_response_failure(resp, 400, "Bad Request");

		// TODO: reservation-token support
		return stun_server_response_failure(resp, 508, "Insufficient Capacity");
	}

	// 6. check EVEN-PORT
	if (even_port != 0)
	{
		// TODO: event-port support
		return stun_server_response_failure(resp, 508, "Insufficient Capacity");
	}

	// 7.
	//stun_message_add_error(&resp->msg, 486, "Allocation Quota Reached");

	memcpy(&allocate->auth, &req->auth, sizeof(struct stun_credential_t));
	memcpy(&allocate->addr, &req->addr, sizeof(struct stun_address_t));
	
	// In all cases, the server SHOULD only allocate ports from the range
	// 49152 - 65535 (the Dynamic and/or Private Port range [Port-Numbers]),
	// https://www.ncftp.com/ncftpd/doc/misc/ephemeral_ports.html
	// sudo sysctl -w net.ipv4.ip_local_port_range="60000 61000" 

	// reply
	r = 0 == r ? stun_message_add_uint32(&resp->msg, STUN_ATTR_LIFETIME, lifetime) : r;
	r = 0 == r ? stun_message_add_address(&resp->msg, STUN_ATTR_XOR_MAPPED_ADDRESS, &allocate->addr.peer) : r;
	return 0 == r ? resp->stun->handler.onallocate(resp->stun->param, resp, req) : r;
}

int turn_agent_allocate_response(struct stun_response_t* resp, const struct sockaddr_storage* relay, int code, const char* pharse)
{
	int r;
	struct stun_message_t* msg;
	struct turn_allocation_t* allocate;

	msg = &resp->msg;
	allocate = turn_agent_allocation_find_by_address(&resp->stun->turnservers, &resp->addr.host, &resp->addr.peer);
	if (!allocate)
		return stun_server_response_failure(resp, 437, "Allocation Mismatch");

	if (code < 300)
	{
		// alloc relayed transport address
		allocate->expire += system_clock();
		memcpy(&allocate->addr.relay, relay, sizeof(struct sockaddr_storage));
		r = stun_message_add_address(&resp->msg, STUN_ATTR_XOR_RELAYED_ADDRESS, &allocate->addr.relay);

		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_ALLOCATE);
		r = 0 == r ? stun_message_add_credentials(msg, &resp->auth) : r;
		r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	}
	else
	{
		turn_agent_allocation_remove(resp->stun, allocate);
		turn_allocation_destroy(&allocate);

		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_ALLOCATE);
		r = stun_message_add_error(&resp->msg, code, pharse);
	}

	return 0 == r ? stun_response_send(resp->stun, resp) : r;
}

int turn_agent_onrefresh(const struct stun_request_t* req, struct stun_response_t* resp)
{
	uint32_t lifetime;
	const struct stun_attr_t* attr;
	struct turn_allocation_t* allocate;

	allocate = turn_agent_allocation_find_by_address(&resp->stun->turnservers, &req->addr.host, &req->addr.peer);
	if (NULL != allocate)
		return stun_server_response_failure(resp, 437, "Allocation Mismatch");

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_LIFETIME);
	lifetime = attr ? attr->v.u32 : 0;

	if (0 == lifetime)
	{
		turn_agent_allocation_remove(resp->stun, allocate);
		turn_allocation_destroy(&allocate);
	}
	else
	{
		lifetime = lifetime > TURN_LIFETIME ? lifetime : TURN_LIFETIME;
		lifetime = lifetime < 3600 ? lifetime : 3600; // 1-hour
		allocate->expire = system_clock() + lifetime * 1000;
	}

	// reply
	stun_message_add_uint32(&resp->msg, STUN_ATTR_LIFETIME, lifetime);
	return resp->stun->handler.onrefresh(resp->stun->param, resp, req, lifetime);
}

int turn_agent_refresh_response(struct stun_response_t* resp, int code, const char* pharse)
{
	int r;
	struct stun_message_t* msg;
	msg = &resp->msg;
	if (code < 300)
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_REFRESH);
		r = stun_message_add_credentials(msg, &resp->auth);
		r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	}
	else
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_REFRESH);
		r = stun_message_add_error(&resp->msg, code, pharse);
	}

	return 0 == r ? stun_response_send(resp->stun, resp) : r;
}

static int turn_agent_add_permission(void* param, const struct stun_attr_t* attr)
{
	struct turn_allocation_t* allocate;
	allocate = (struct turn_allocation_t*)param;
	return turn_allocation_add_permission(allocate, &attr->v.addr);
}

int turn_agent_oncreate_permission(const struct stun_request_t* req, struct stun_response_t* resp)
{
	int r;
	struct turn_allocation_t* allocate;
	struct turn_permission_t* permission;

	allocate = turn_agent_allocation_find_by_address(&resp->stun->turnservers, &req->addr.host, &req->addr.peer);
	if (NULL != allocate)
		return stun_server_response_failure(resp, 437, "Allocation Mismatch");

	// The CreatePermission request MUST contain at least one XOR-PEER-ADDRESS 
	// attribute and MAY contain multiple such attributes
	r = stun_message_attr_list(&req->msg, STUN_ATTR_XOR_PEER_ADDRESS, turn_agent_add_permission, allocate);
	if (0 != r)
	{
		// TODO:
		// restrictions on the IP address allowed in the XOR-PEER-ADDRESS attribute
		//stun_message_add_error(&resp->msg, 403, "Forbidden)");

		return stun_server_response_failure(resp, 508, "Insufficient Capacity)");
	}

	if(0 == darray_count(&allocate->permissions))
		return stun_server_response_failure(resp, 400, "Bad Request");

	// reply
	permission = (struct turn_permission_t*)darray_get(&allocate->permissions, 0);
	return resp->stun->handler.onpermission(resp->stun->param, resp, req, &permission->addr);
}

int turn_agent_create_permission_response(struct stun_response_t* resp, int code, const char* pharse)
{
	int r;
	struct stun_message_t* msg;
	msg = &resp->msg;
	if (code < 300)
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_CREATE_PERMISSION);
		r = stun_message_add_credentials(msg, &resp->auth);
		r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	}
	else
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_CREATE_PERMISSION);
		r = stun_message_add_error(&resp->msg, code, pharse);
	}

	r = stun_message_add_credentials(msg, &resp->auth);
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_response_send(resp->stun, resp) : r;
}

int turn_agent_onchannel_bind(const struct stun_request_t* req, struct stun_response_t* resp)
{
	int r;
	const struct stun_attr_t* peer;
	const struct stun_attr_t* channel;
	struct turn_allocation_t* allocate;

	allocate = turn_agent_allocation_find_by_address(&resp->stun->turnservers, &req->addr.host, &req->addr.peer);
	if (NULL != allocate)
		return stun_server_response_failure(resp, 437, "Allocation Mismatch");

	peer = stun_message_attr_find(&req->msg, STUN_ATTR_XOR_PEER_ADDRESS);
	channel = stun_message_attr_find(&req->msg, STUN_ATTR_CHANNEL_NUMBER);
	if (!peer || !channel || (channel->v.u32 >> 16) < 0x4000 || (channel->v.u32 >> 16) > 0x7FFE)
		return stun_server_response_failure(resp, 400, "Bad Request");

	//stun_message_add_error(&resp->msg, 400, "Bad Request");

	// restrictions on the IP address allowed in the XOR-PEER-ADDRESS attribute
	//stun_message_add_error(&resp->msg, 403, "Forbidden)");

	// A ChannelBind transaction also creates or refreshes a permission towards the peer
	r = turn_allocation_add_permission(allocate, &peer->v.addr);
	r = 0 == r ? turn_allocation_add_channel(allocate, &peer->v.addr, channel->v.u32 >> 16) : r;
	if (0 != r)
		return stun_server_response_failure(resp, 508, "Insufficient Capacity)");

	// reply
	return resp->stun->handler.onchannel(resp->stun->param, resp, req, &peer->v.addr, (channel->v.u32) >> 16);
}

int turn_agent_channel_bind_response(struct stun_response_t* resp, int code, const char* pharse)
{
	int r;
	struct stun_message_t* msg;
	msg = &resp->msg;
	if (code < 300)
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_CHANNEL_BIND);
		r = stun_message_add_credentials(msg, &resp->auth);
		r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	}
	else
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_CHANNEL_BIND);
		r = stun_message_add_error(&resp->msg, code, pharse);
	}

	r = stun_message_add_credentials(msg, &resp->auth);
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_response_send(resp->stun, resp) : r;
}

/// turn server relay peer to client by DATA indication
static int turn_server_relay_data(struct stun_agent_t* turn, const struct turn_allocation_t* allocate, const struct sockaddr_storage* peer, const void* data, int bytes)
{
	int r;
	struct stun_message_t msg;
	
	memset(&msg, 0, sizeof(struct stun_message_t));
	msg.header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_INDICATION, STUN_METHOD_DATA);
	msg.header.length = 0;
	msg.header.cookie = STUN_MAGIC_COOKIE;
	r = stun_transaction_id(msg.header.tid, sizeof(msg.header.tid));

	r = 0 == r ? stun_message_add_address(&msg, STUN_ATTR_XOR_PEER_ADDRESS, peer) : r;
	r = 0 == r ? stun_message_add_data(&msg, STUN_ATTR_DATA, data, bytes) : r;
	r = 0 == r ? stun_message_add_credentials(&msg, &allocate->auth) : r;
	r = 0 == r ? stun_message_add_fingerprint(&msg) : r;

	// STUN indications are not retransmitted
	r = 0 == r ? stun_message_send(turn, &msg, allocate->addr.protocol, &allocate->addr.host, &allocate->addr.peer) : r;
	return r;
}

/// turn server relay peer to client by ChannelData
static int turn_server_relay_channel_data(struct stun_agent_t* turn, const struct turn_allocation_t* allocate, const struct turn_channel_t* channel, const void* data, int bytes)
{
	uint8_t ptr[1600];

	if (channel->expired < system_clock())
		return 0; // expired

	if (bytes + 4 > sizeof(ptr))
		return -1; // MTU too long

	ptr[0] = (uint8_t)(channel->channel >> 8);
	ptr[1] = (uint8_t)(channel->channel);
	ptr[2] = (uint8_t)(bytes >> 8);
	ptr[3] = (uint8_t)(bytes);
	memcpy(ptr + 4, data, bytes);

	return turn->handler.send(turn->param, allocate->addr.protocol, &allocate->addr.host, &allocate->addr.peer, ptr, bytes+4);
}

int turn_agent_relay(struct stun_agent_t* turn, const struct turn_allocation_t* allocate, const struct sockaddr_storage* peer, const void* data, int bytes)
{
	const struct turn_channel_t* channel;
	const struct turn_permission_t* permission;

	permission = turn_allocation_find_permission(allocate, peer);
	if (!permission || permission->expired < system_clock())
		return 0; // expired

	channel = turn_allocation_find_channel_by_peer(allocate, peer);
	return channel ? turn_server_relay_channel_data(turn, allocate, channel, data, bytes) : turn_server_relay_data(turn, allocate, peer, data, bytes);
}

int turn_server_onsend(struct stun_agent_t* turn, const struct stun_request_t* req)
{
	int fragment;
	struct sockaddr_storage peer;
	const struct stun_attr_t* attr;
	struct turn_allocation_t* allocate;
	
	allocate = turn_agent_allocation_find_by_address(&req->stun->turnservers, &req->addr.host, &req->addr.peer);
	if (NULL == allocate || allocate->expire < system_clock())
		return 0; // discard

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_DONT_FRAGMENT);
	fragment = attr ? attr->v.u8 : 0;

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_XOR_PEER_ADDRESS);
	if (!attr) return 0; // discard
	memcpy(&peer, &attr->v.addr, sizeof(struct sockaddr_storage));

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_DATA);
	if (!attr) return 0; // discard

	return turn->handler.send(turn->param, allocate->peertransport, &allocate->addr.relay, &peer, attr->v.ptr, attr->length);
}

// ChannelData from client
int turn_server_onchannel_data(struct stun_agent_t* turn, struct turn_allocation_t* allocate, const uint8_t* data, int bytes)
{
	uint16_t number;
	uint16_t length;
	const struct turn_channel_t* channel;

	number = ((uint16_t)data[0] << 8) | (uint16_t)data[1];
	length = ((uint16_t)data[2] << 8) | (uint16_t)data[3];
	channel = turn_allocation_find_channel(allocate, number);
	if (channel && length + 4 <= bytes)
		turn->handler.send(turn->param, allocate->peertransport, &allocate->addr.relay, &channel->addr, (const uint8_t*)data + 4, length);
	
	return 0;
}
