#include "stun-internal.h"
#include "turn-internal.h"
#include "sys/system.h"

static int stun_server_response_failure(struct stun_response_t* resp, int code, const char* phrase)
{
	struct stun_message_t *msg;

	msg = &resp->msg;
	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_MESSAGE_METHOD(msg->header.msgtype));

	stun_message_add_error(msg, code, phrase);
	return stun_response_send(resp->stun, resp);
}

static int turn_server_allocate_doresponse(struct stun_response_t* resp, struct turn_allocation_t* allocate)
{
	resp->msg.header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_ALLOCATE);
	stun_message_add_uint32(&resp->msg, STUN_ATTR_LIFETIME, allocate->lifetime);
	stun_message_add_address(&resp->msg, STUN_ATTR_XOR_MAPPED_ADDRESS, (const struct sockaddr*)&allocate->addr.peer);
	stun_message_add_address(&resp->msg, STUN_ATTR_XOR_RELAYED_ADDRESS, (const struct sockaddr*)&allocate->addr.relay);
	if (allocate->token[0])
		stun_message_add_data(&resp->msg, STUN_ATTR_RESERVATION_TOKEN, allocate->token, sizeof(allocate->token));
	stun_message_add_credentials(&resp->msg, &resp->auth);
	stun_message_add_fingerprint(&resp->msg);
	return stun_response_send(resp->stun, resp);
}

// rfc5766 6.2. Receiving an Allocate Request (p24)
int turn_server_onallocate(struct stun_agent_t* turn, const struct stun_request_t* req, struct stun_response_t* resp)
{
	const struct stun_attr_t* token;
	const struct stun_attr_t* family;
	const struct stun_attr_t* lifetime;
    const struct stun_attr_t* evenport;
	const struct stun_attr_t* fragment;
	const struct stun_attr_t* transport;
	struct turn_allocation_t* allocate;
    struct sockaddr_storage relay;

	// 1. long-term credential auth

	// 2. checks the 5-tuple
	allocate = turn_agent_allocation_find_by_address(&turn->turnservers, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
	if (NULL != allocate)
		return allocate->expire < system_clock() ? stun_server_response_failure(resp, 437, "Allocation Mismatch") : turn_server_allocate_doresponse(resp, allocate);

	// 3. check REQUESTED-TRANSPORT
	transport = stun_message_attr_find(&req->msg, STUN_ATTR_REQUESTED_TRANSPORT);
	if (transport && TURN_TRANSPORT_UDP != (transport->v.u32 >> 24))
		return stun_server_response_failure(resp, 442, "Unsupported Transport Protocol");

	// 6. check EVEN-PORT
	evenport = stun_message_attr_find(&req->msg, STUN_ATTR_EVEN_PORT);
	lifetime = stun_message_attr_find(&req->msg, STUN_ATTR_LIFETIME);
	fragment = stun_message_attr_find(&req->msg, STUN_ATTR_DONT_FRAGMENT);

	// 7.
	//stun_message_add_error(&resp->msg, 486, "Allocation Quota Reached");

	// 8. check request address family
	family = stun_message_attr_find(&req->msg, STUN_ATTR_REQUESTED_ADDRESS_FAMILY);
	if (family)
	{
		if (0x01 != family->v.u32 >> 24 && 0x02 != family->v.u32 >> 24)
			return stun_server_response_failure(resp, 440, "Address Family not Supported");
	}

	// 5. check RESERVATION-TOKEN
	token = stun_message_attr_find(&req->msg, STUN_ATTR_RESERVATION_TOKEN);
	if (token)
	{
		if (evenport)
			return stun_server_response_failure(resp, 400, "Bad Request");

		allocate = turn_agent_allocation_find_by_token(&turn->turnreserved, (void*)*(intptr_t*)token->v.ptr);
		if (!allocate)
		{
			// unknown token, discard
			stun_response_destroy(&resp);
			return 0;
		}
		else if (allocate->expire < system_clock())
		{
			return stun_server_response_failure(resp, 508, "Insufficient Capacity");
		}

		allocate->lifetime = lifetime ? lifetime->v.u32 : TURN_LIFETIME;
		allocate->lifetime = allocate->lifetime > TURN_LIFETIME ? allocate->lifetime : TURN_LIFETIME;
		allocate->lifetime = allocate->lifetime < 3600 ? allocate->lifetime : 3600; // 1-hour
		allocate->expire = system_clock() + allocate->lifetime * 1000;
		allocate->dontfragment = fragment ? fragment->v.u32 : 1;
		allocate->peertransport = !transport || TURN_TRANSPORT_UDP == (transport->v.u32 >> 24) ? STUN_PROTOCOL_UDP : STUN_PROTOCOL_TCP;

		memcpy(&relay, &allocate->addr.relay, sizeof(struct sockaddr_storage)); // don't overwrite addr.relay
		memcpy(&allocate->addr, &req->addr, sizeof(struct stun_address_t));
		memcpy(&allocate->auth, &req->auth, sizeof(struct stun_credential_t));
		memmove(&allocate->addr.relay, &relay, socket_addr_len((struct sockaddr*)&relay)); // token relay address overwrite

		turn_agent_allocation_remove(&resp->stun->turnreserved, allocate);
		turn_agent_allocation_insert(&resp->stun->turnservers, allocate);
		return turn_server_allocate_doresponse(resp, allocate);
	}
	else if(turn->handler.onallocate)
	{
		allocate = turn_allocation_create();
		if (!allocate)
			return stun_server_response_failure(resp, 486, "Allocation Quota Reached");
	
		if (evenport)
			allocate->reserve_next_higher_port = evenport->v.u8 & 0x80;

		allocate->lifetime = lifetime ? lifetime->v.u32 : TURN_LIFETIME;
		allocate->lifetime = allocate->lifetime > TURN_LIFETIME ? allocate->lifetime : TURN_LIFETIME;
		allocate->lifetime = allocate->lifetime < 3600 ? allocate->lifetime : 3600; // 1-hour
		allocate->expire = system_clock() + allocate->lifetime * 1000;
		allocate->dontfragment = fragment ? fragment->v.u32 : 1;
		allocate->peertransport = !transport || TURN_TRANSPORT_UDP == (transport->v.u32 >> 24) ? STUN_PROTOCOL_UDP : STUN_PROTOCOL_TCP;

		// In all cases, the server SHOULD only allocate ports from the range
		// 49152 - 65535 (the Dynamic and/or Private Port range [Port-Numbers]),
		// https://www.ncftp.com/ncftpd/doc/misc/ephemeral_ports.html
		// sudo sysctl -w net.ipv4.ip_local_port_range="60000 61000" 

		memcpy(&allocate->addr, &req->addr, sizeof(struct stun_address_t));
		memcpy(&allocate->auth, &req->auth, sizeof(struct stun_credential_t));
		turn_agent_allocation_insert(&turn->turnreserved, allocate);
		return turn->handler.onallocate(turn->param, resp, req, evenport ? 1 : 0, allocate->reserve_next_higher_port);
	}
	else
	{
		// discard
		return 0;
	}
}

int turn_agent_allocate_response(struct stun_response_t* resp, const struct sockaddr* relay, int code, const char* pharse)
{
	struct stun_message_t* msg;
	struct turn_allocation_t* allocate;

	msg = &resp->msg;
	allocate = turn_agent_allocation_find_by_address(&resp->stun->turnreserved, (const struct sockaddr*)&resp->addr.host, (const struct sockaddr*)&resp->addr.peer);
	if (!allocate)
		return stun_server_response_failure(resp, 437, "Allocation Mismatch");
	assert(NULL == turn_agent_allocation_find_by_relay(&resp->stun->turnservers, relay));
	turn_agent_allocation_remove(&resp->stun->turnreserved, allocate);

	if (code < 300)
	{
		// alloc relayed transport address
		memmove(&allocate->addr.relay, relay, socket_addr_len(relay)); // token relay address overwrite
		turn_agent_allocation_insert(&resp->stun->turnservers, allocate);
		return turn_server_allocate_doresponse(resp, allocate);
	}
	else
	{
		turn_allocation_destroy(&allocate);

		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_ALLOCATE);
		stun_message_add_error(&resp->msg, code, pharse);
		return stun_response_send(resp->stun, resp);
	}
}

int turn_server_onrefresh(struct stun_agent_t* turn, const struct stun_request_t* req, struct stun_response_t* resp)
{
	uint32_t lifetime;
	const struct stun_attr_t* attr;
	struct turn_allocation_t* allocate;

	allocate = turn_agent_allocation_find_by_address(&turn->turnservers, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
	if (NULL == allocate)
		return stun_server_response_failure(resp, 437, "Allocation Mismatch");

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_LIFETIME);
	lifetime = attr ? attr->v.u32 : 0;

	if (0 == lifetime)
	{
		turn_agent_allocation_remove(&turn->turnservers, allocate);
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
	return turn->handler.onrefresh(turn->param, resp, req, lifetime);
}

int turn_agent_refresh_response(struct stun_response_t* resp, int code, const char* pharse)
{
	struct stun_message_t* msg;
	msg = &resp->msg;
	if (code < 300)
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_REFRESH);
		stun_message_add_credentials(msg, &resp->auth);
		stun_message_add_fingerprint(msg);
	}
	else
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_REFRESH);
		stun_message_add_error(&resp->msg, code, pharse);
	}

	return stun_response_send(resp->stun, resp);
}

static int turn_agent_add_permission(void* param, const struct stun_attr_t* attr)
{
	struct turn_allocation_t* allocate;
	allocate = (struct turn_allocation_t*)param;
	return turn_allocation_add_permission(allocate, (const struct sockaddr*)&attr->v.addr);
}

int turn_server_oncreate_permission(struct stun_agent_t* turn, const struct stun_request_t* req, struct stun_response_t* resp)
{
	int r, n;
	struct turn_allocation_t* allocate;
	struct turn_permission_t* permission;

	allocate = turn_agent_allocation_find_by_address(&turn->turnservers, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
	if (NULL == allocate)
		return stun_server_response_failure(resp, 437, "Allocation Mismatch");

	n = allocate->npermission;
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

	if(n >= allocate->npermission)
		return stun_server_response_failure(resp, 400, "Bad Request");

	// reply
	permission = &allocate->permissions[n];
    if(permission->addr.ss_family != allocate->addr.relay.ss_family)
        return stun_server_response_failure(resp, 443, "Peer Address Family Mismatch");

	return turn->handler.onpermission(turn->param, resp, req, (const struct sockaddr*)&permission->addr);
}

int turn_agent_create_permission_response(struct stun_response_t* resp, int code, const char* pharse)
{
	struct stun_message_t* msg;
	msg = &resp->msg;
	if (code < 300)
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_CREATE_PERMISSION);
		stun_message_add_credentials(msg, &resp->auth);
		stun_message_add_fingerprint(msg);
	}
	else
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_CREATE_PERMISSION);
		stun_message_add_error(&resp->msg, code, pharse);
	}

	return stun_response_send(resp->stun, resp);
}

int turn_server_onchannel_bind(struct stun_agent_t* turn, const struct stun_request_t* req, struct stun_response_t* resp)
{
	int r;
	const struct stun_attr_t* peer;
	const struct stun_attr_t* channel;
	struct turn_allocation_t* allocate;

	allocate = turn_agent_allocation_find_by_address(&turn->turnservers, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
	if (NULL == allocate)
		return stun_server_response_failure(resp, 437, "Allocation Mismatch");

	peer = stun_message_attr_find(&req->msg, STUN_ATTR_XOR_PEER_ADDRESS);
	channel = stun_message_attr_find(&req->msg, STUN_ATTR_CHANNEL_NUMBER);
	if (!peer || !channel || (channel->v.u32 >> 16) < 0x4000 || (channel->v.u32 >> 16) > 0x7FFE)
		return stun_server_response_failure(resp, 400, "Bad Request");

	//stun_message_add_error(&resp->msg, 400, "Bad Request");

	// restrictions on the IP address allowed in the XOR-PEER-ADDRESS attribute
	//stun_message_add_error(&resp->msg, 403, "Forbidden)");
    
    if(peer->v.addr.ss_family != allocate->addr.relay.ss_family)
        return stun_server_response_failure(resp, 443, "Peer Address Family Mismatch");

	// A ChannelBind transaction also creates or refreshes a permission towards the peer
	r = turn_allocation_add_permission(allocate, (const struct sockaddr*)&peer->v.addr);
	r = 0 == r ? turn_allocation_add_channel(allocate, (const struct sockaddr*)&peer->v.addr, channel->v.u32 >> 16) : r;
	if (0 != r)
		return stun_server_response_failure(resp, 508, "Insufficient Capacity)");

	// reply
	return turn->handler.onchannel(turn->param, resp, req, (const struct sockaddr*)&peer->v.addr, (channel->v.u32) >> 16);
}

int turn_agent_channel_bind_response(struct stun_response_t* resp, int code, const char* pharse)
{
	struct stun_message_t* msg;
	msg = &resp->msg;
	if (code < 300)
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_SUCCESS_RESPONSE, STUN_METHOD_CHANNEL_BIND);
		stun_message_add_credentials(msg, &resp->auth);
		stun_message_add_fingerprint(msg);
	}
	else
	{
		msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_METHOD_CHANNEL_BIND);
		stun_message_add_error(&resp->msg, code, pharse);
	}

	return stun_response_send(resp->stun, resp);
}

/// turn server relay peer to client by DATA indication(peer -> server -> client)
static int turn_server_relay_data(struct stun_agent_t* turn, const struct turn_allocation_t* allocate, const struct sockaddr* peer, const void* data, int bytes)
{
	struct stun_message_t msg;
	
	memset(&msg, 0, sizeof(struct stun_message_t));
	msg.header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_INDICATION, STUN_METHOD_DATA);
	msg.header.length = 0;
	msg.header.cookie = STUN_MAGIC_COOKIE;
	stun_transaction_id(msg.header.tid, sizeof(msg.header.tid));

	stun_message_add_address(&msg, STUN_ATTR_XOR_PEER_ADDRESS, peer);
	stun_message_add_data(&msg, STUN_ATTR_DATA, data, bytes);
	stun_message_add_credentials(&msg, &allocate->auth);
	stun_message_add_fingerprint(&msg);

	// STUN indications are not retransmitted
	return stun_message_send(turn, &msg, allocate->addr.protocol, &allocate->addr.host, &allocate->addr.peer, NULL);
}

/// turn server relay peer to client by ChannelData(peer -> server -> client)
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

	return turn->handler.send(turn->param, allocate->addr.protocol, (const struct sockaddr*)&allocate->addr.host, (const struct sockaddr*)&allocate->addr.peer, ptr, bytes+4);
}

// relay peer data to client (peer -> server -> client)
int turn_server_relay(struct stun_agent_t* turn, const struct turn_allocation_t* allocate, const struct sockaddr* peer, const void* data, int bytes)
{
	const struct turn_channel_t* channel;
	const struct turn_permission_t* permission;

	permission = turn_allocation_find_permission(allocate, peer);
	if (!permission || permission->expired < system_clock())
		return 0; // expired

	channel = turn_allocation_find_channel_by_peer(allocate, peer);
	return channel ? turn_server_relay_channel_data(turn, allocate, channel, data, bytes) : turn_server_relay_data(turn, allocate, peer, data, bytes);
}

// send data to peer (client -> server -> peer)
int turn_server_onsend(struct stun_agent_t* turn, const struct stun_request_t* req)
{
	int fragment;
	struct sockaddr_storage peer;
	const struct stun_attr_t* attr;
	struct turn_allocation_t* allocate;
	
	allocate = turn_agent_allocation_find_by_address(&turn->turnservers, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
	if (NULL == allocate || allocate->expire < system_clock())
		return 0; // discard

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_DONT_FRAGMENT);
	fragment = attr ? attr->v.u8 : 0;

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_XOR_PEER_ADDRESS);
	if (!attr) return 0; // discard
	memcpy(&peer, &attr->v.addr, sizeof(struct sockaddr_storage));

	attr = stun_message_attr_find(&req->msg, STUN_ATTR_DATA);
	if (!attr) return 0; // discard

	return turn->handler.send(turn->param, allocate->peertransport, (const struct sockaddr*)&allocate->addr.relay, (const struct sockaddr*)&peer, attr->v.ptr, attr->length);
}

// ChannelData from client(client -> server -> peer)
int turn_server_onchannel_data(struct stun_agent_t* turn, struct turn_allocation_t* allocate, const uint8_t* data, int bytes)
{
	uint16_t number;
	uint16_t length;
	const struct turn_channel_t* channel;

	number = ((uint16_t)data[0] << 8) | (uint16_t)data[1];
	length = ((uint16_t)data[2] << 8) | (uint16_t)data[3];
	channel = turn_allocation_find_channel(allocate, number);
	if (channel && length + 4 <= bytes)
		turn->handler.send(turn->param, allocate->peertransport, (const struct sockaddr*)&allocate->addr.relay, (const struct sockaddr*)&channel->addr, (const uint8_t*)data + 4, length);
	
	return 0;
}
