#include "stun-agent.h"
#include "stun-internal.h"
#include "turn-internal.h"
#include "sys/system.h"

// rfc3489 8.2 Shared Secret Requests (p13)
// rfc3489 9.3 Formulating the Binding Request (p17)
int turn_agent_allocate(stun_request_t* req, turn_agent_ondata ondata, void* param)
{
	int r;
	struct stun_message_t* msg;
	struct turn_allocation_t* allocate;
	msg = &req->msg;

	req->ondata = ondata;
	req->ondataparam = param;
	if (req->auth.credential != STUN_CREDENTIAL_LONG_TERM)
	{
		assert(0);
		return -1;
	}

	allocate = turn_agent_allocation_find_by_address(&req->stun->turnclients, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
	if (!allocate)
	{
		allocate = turn_allocation_create();
		if (!allocate) return -1;
		turn_agent_allocation_insert(req->stun, allocate);
	}

	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_ALLOCATE);
	r = stun_message_add_uint32(msg, STUN_ATTR_REQUESTED_TRANSPORT, TURN_TRANSPORT_UDP << 24);
	r = 0 == r ? stun_message_add_uint32(msg, STUN_ATTR_LIFETIME, TURN_LIFETIME) : r;
	r = 0 == r ? stun_message_add_flag(msg, STUN_ATTR_DONT_FRAGMENT) : r;
	//	r = 0 == r ? stun_message_add_uint8(msg, STUN_ATTR_EVEN_PORT, 0x80) : r;

	r = 0 == r ? stun_message_add_credentials(msg, &req->auth) : r;
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_request_send(req->stun, req) : r;
}

int turn_client_allocate_onresponse(struct stun_request_t* req, const struct stun_request_t* resp)
{
	const struct stun_attr_t* attr;
	struct stun_agent_t* stun;
	struct turn_allocation_t* allocate;
	stun = req->stun;

	allocate = turn_agent_allocation_find_by_address(&stun->turnclients, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
	if (!allocate)
	{
		allocate = turn_allocation_create();
		if (!allocate) return -1;
		turn_agent_allocation_insert(stun, allocate);
	}

	memcpy(&allocate->auth, &req->auth, sizeof(struct stun_credential_t));
	memcpy(&allocate->addr, &req->addr, sizeof(struct stun_address_t)); // fill host/peer
	allocate->ondataparam = req->ondataparam;
	allocate->ondata = req->ondata;
	
	attr = stun_message_attr_find(&resp->msg, STUN_ATTR_XOR_RELAYED_ADDRESS);
	if (!attr) goto FAILED;
	memcpy(&allocate->addr.relay, &attr->v.addr, sizeof(struct sockaddr_storage));

	attr = stun_message_attr_find(&resp->msg, STUN_ATTR_XOR_MAPPED_ADDRESS);
	if (!attr) goto FAILED;
	memcpy(&allocate->addr.reflexive, &resp->addr.host, sizeof(struct sockaddr_storage));

	attr = stun_message_attr_find(&resp->msg, STUN_ATTR_LIFETIME);
	allocate->expire = system_clock() + (attr ? attr->v.u32 : TURN_LIFETIME) * 1000;

	return 0;
FAILED:
	turn_agent_allocation_remove(stun, allocate);
	turn_allocation_destroy(&allocate);
	return -1;
}

int turn_agent_refresh(stun_request_t* req, int expired)
{
	int r;
	struct stun_message_t* msg;
	msg = &req->msg;

	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_REFRESH);
	r = stun_message_add_uint32(msg, STUN_ATTR_LIFETIME, expired);

	r = 0 == r ? stun_message_add_credentials(msg, &req->auth) : r;
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_request_send(req->stun, req) : r;
}

int turn_client_refresh_onresponse(struct stun_request_t* req, const struct stun_request_t* resp)
{
	const struct stun_attr_t* attr;
	struct stun_agent_t* stun;
	struct turn_allocation_t* allocate;
	stun = req->stun;

	attr = stun_message_attr_find(&resp->msg, STUN_ATTR_LIFETIME);

	allocate = turn_agent_allocation_find_by_address(&stun->turnclients, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
	if (!allocate)
		return (!attr || 0 == attr->v.u32) ? 0 : -1;

	if (!attr || 0 == attr->v.u32)
	{
		turn_agent_allocation_remove(stun, allocate);
		turn_allocation_destroy(&allocate);
		return 0;
	}

	allocate->expire = system_clock() + (attr ? attr->v.u32 : TURN_LIFETIME) * 1000;
	return 0;
}

//int turn_agent_create_permission(stun_request_t* req, const struct sockaddr_storage* peers[], int n)
int turn_agent_create_permission(stun_request_t* req, const struct sockaddr* peer)
{
	int r;
	struct stun_message_t* msg;
	msg = &req->msg;

	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_CREATE_PERMISSION);
	
	if (AF_INET != peer->sa_family && AF_INET6 != peer->sa_family)
		return -1;
	r = stun_message_add_address(msg, STUN_ATTR_XOR_PEER_ADDRESS, peer);
	
	r = 0 == r ? stun_message_add_credentials(msg, &req->auth) : r;
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_request_send(req->stun, req) : r;
}

static int turn_client_add_permission(void* param, const struct stun_attr_t* attr)
{
	struct turn_allocation_t* allocate;
	allocate = (struct turn_allocation_t*)param;
	return turn_allocation_add_permission(allocate, (const struct sockaddr*)&attr->v.addr);
}

int turn_client_create_permission_onresponse(struct stun_request_t* req, const struct stun_request_t* resp)
{
	struct stun_agent_t* stun;
	struct turn_allocation_t* allocate;
	stun = req->stun;

	allocate = turn_agent_allocation_find_by_address(&stun->turnclients, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
	if (!allocate) return -1;

	return stun_message_attr_list(&req->msg, STUN_ATTR_XOR_PEER_ADDRESS, turn_client_add_permission, allocate);
}

int turn_agent_channel_bind(stun_request_t* req, const struct sockaddr* peer, uint16_t channel)
{
	int r;
	struct stun_message_t* msg;
	msg = &req->msg;

	if (AF_INET != peer->sa_family && AF_INET6 != peer->sa_family)
		return -1;
	if (channel < 0x4000 || channel > 0x7FFE)
		return -1;

	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_CHANNEL_BIND);
	r = stun_message_add_address(msg, STUN_ATTR_XOR_PEER_ADDRESS, peer);
	r = 0 == r ? stun_message_add_uint32(msg, STUN_ATTR_CHANNEL_NUMBER, channel << 16) : r;

	r = 0 == r ? stun_message_add_credentials(msg, &req->auth) : r;
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;
	return 0 == r ? stun_request_send(req->stun, req) : r;
}

int turn_client_channel_bind_onresponse(struct stun_request_t* req, const struct stun_request_t* resp)
{
	int r;
	const struct stun_attr_t* peer;
	const struct stun_attr_t* channel;
	struct stun_agent_t* stun;
	struct turn_allocation_t* allocate;
	stun = req->stun;

	allocate = turn_agent_allocation_find_by_address(&stun->turnclients, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
	if (!allocate) return -1;

	peer = stun_message_attr_find(&req->msg, STUN_ATTR_XOR_PEER_ADDRESS);
	channel = stun_message_attr_find(&req->msg, STUN_ATTR_CHANNEL_NUMBER);
	assert(peer && channel);

	// A ChannelBind transaction also creates or refreshes a permission towards the peer
	r = turn_allocation_add_permission(allocate, (const struct sockaddr*)&peer->v.addr);
	r = 0 == r ? turn_allocation_add_channel(allocate, (const struct sockaddr*)&peer->v.addr, channel->v.u32 >> 16) : r;
	return r;
}

int turn_agent_send(stun_request_t* req, const struct sockaddr* peer, const void* data, int bytes)
{
	int r;
	struct stun_message_t* msg;
	msg = &req->msg;

	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_INDICATION, STUN_METHOD_SEND);
	r = stun_message_add_flag(msg, STUN_ATTR_DONT_FRAGMENT);
	r = 0 == r ? stun_message_add_address(msg, STUN_ATTR_XOR_PEER_ADDRESS, peer) : r;
	r = 0 == r ? stun_message_add_data(msg, STUN_ATTR_DATA, data, bytes) : r;

	r = 0 == r ? stun_message_add_credentials(msg, &req->auth) : r;
	r = 0 == r ? stun_message_add_fingerprint(msg) : r;

	// STUN indications are not retransmitted
	r = 0 == r ? stun_message_send(req->stun, &req->msg, req->addr.protocol, &req->addr.host, &req->addr.peer) : r;;
	stun_agent_remove(req->stun, req);
	return r;
}

int turn_client_ondata(struct stun_agent_t* turn, const struct stun_request_t* resp)
{
	const struct stun_attr_t* data;
	const struct stun_attr_t* peer;
	struct turn_allocation_t* allocate;
	
	peer = stun_message_attr_find(&resp->msg, STUN_ATTR_XOR_PEER_ADDRESS);
	data = stun_message_attr_find(&resp->msg, STUN_ATTR_DATA);
	if (!peer || !data) return 0; // discard

	allocate = turn_agent_allocation_find_by_address(&turn->turnclients, (const struct sockaddr*)&resp->addr.host, (const struct sockaddr*)&resp->addr.peer);
	if (!allocate) return 0; // discard

	// TODO: check allocation lifetime ???

	allocate->ondata(allocate->ondataparam, data->v.ptr, data->length, resp->addr.protocol, (const struct sockaddr*)&resp->addr.host, (const struct sockaddr*)&peer->v.addr);
	return 0;
}

// ChannelData from server
int turn_client_onchannel_data(struct stun_agent_t* stun, struct turn_allocation_t* allocate, const uint8_t* data, int bytes)
{
	uint16_t number;
	uint16_t length;
	const struct turn_channel_t* channel;

	// TODO: TURN data
	// check permission
	number = ((uint16_t)data[0] << 8) | (uint16_t)data[1];
	length = ((uint16_t)data[2] << 8) | (uint16_t)data[3];
	channel = turn_allocation_find_channel(allocate, number);
	if (channel && length + 4 <= bytes)
		allocate->ondata(allocate->ondataparam, (const uint8_t*)data + 4, length, allocate->addr.protocol, (const struct sockaddr*)&allocate->addr.host, (const struct sockaddr*)&channel->addr);
	
	return 0;
}
