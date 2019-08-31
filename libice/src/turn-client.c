#include "stun-internal.h"
#include "turn-internal.h"
#include "sys/system.h"
#include <assert.h>

// rfc3489 8.2 Shared Secret Requests (p13)
// rfc3489 9.3 Formulating the Binding Request (p17)
int turn_agent_allocate(stun_request_t* req, int peertransport)
{
	struct stun_message_t* msg;
	msg = &req->msg;

	if (req->auth.credential != STUN_CREDENTIAL_LONG_TERM)
	{
		assert(0);
		return -1;
	}

	assert(peertransport == TURN_TRANSPORT_UDP);
	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_ALLOCATE);
	stun_message_add_uint32(msg, STUN_ATTR_REQUESTED_TRANSPORT, TURN_TRANSPORT_UDP << 24);
	stun_message_add_uint32(msg, STUN_ATTR_LIFETIME, TURN_LIFETIME);
	//stun_message_add_flag(msg, STUN_ATTR_DONT_FRAGMENT);
	//stun_message_add_uint8(msg, STUN_ATTR_EVEN_PORT, 0x80);

	stun_message_add_credentials(msg, &req->auth);
	stun_message_add_fingerprint(msg);
	return stun_request_send(req->stun, req);
}

int turn_client_allocate_onresponse(struct stun_agent_t* stun, struct stun_request_t* req, const struct stun_message_t* resp)
{
	const struct stun_attr_t* attr;
	struct turn_allocation_t* allocate;

	allocate = turn_agent_allocation_find_by_address(&stun->turnclients, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
	if (!allocate)
	{
		allocate = turn_allocation_create();
		if (!allocate) return -1;
		turn_agent_allocation_insert(&stun->turnclients, allocate);
	}

	memcpy(&allocate->auth, &req->auth, sizeof(struct stun_credential_t));
	memcpy(&allocate->addr, &req->addr, sizeof(struct stun_address_t)); // fill host/peer

	attr = stun_message_attr_find(resp, STUN_ATTR_REQUESTED_TRANSPORT);
	if(!attr) attr = stun_message_attr_find(&req->msg, STUN_ATTR_REQUESTED_TRANSPORT);
	allocate->peertransport = attr ? (attr->v.u32 >> 24) : TURN_TRANSPORT_UDP;

	attr = stun_message_attr_find(resp, STUN_ATTR_XOR_RELAYED_ADDRESS);
	if (!attr) goto FAILED;
	memcpy(&allocate->addr.relay, &attr->v.addr, sizeof(struct sockaddr_storage));

	attr = stun_message_attr_find(resp, STUN_ATTR_XOR_MAPPED_ADDRESS);
	if (!attr) attr = stun_message_attr_find(resp, STUN_ATTR_MAPPED_ADDRESS);
	if (!attr) goto FAILED;
	memcpy(&allocate->addr.reflexive, &attr->v.addr, sizeof(struct sockaddr_storage));

	attr = stun_message_attr_find(resp, STUN_ATTR_LIFETIME);
	allocate->lifetime = attr ? attr->v.u32 : TURN_LIFETIME;
	allocate->expire = system_clock() + allocate->lifetime * 1000;

	return 0;
FAILED:
	turn_agent_allocation_remove(&stun->turnclients, allocate);
	turn_allocation_destroy(&allocate);
	return -1;
}

int turn_agent_refresh(stun_request_t* req, int expired)
{
	struct stun_message_t* msg;
	msg = &req->msg;

	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_REFRESH);
	stun_message_add_uint32(msg, STUN_ATTR_LIFETIME, expired);

	stun_message_add_credentials(msg, &req->auth);
	stun_message_add_fingerprint(msg);
	return stun_request_send(req->stun, req);
}

int turn_client_refresh_onresponse(struct stun_agent_t* stun, struct stun_request_t* req, const struct stun_message_t* resp)
{
	const struct stun_attr_t* attr;
	struct turn_allocation_t* allocate;
	
	attr = stun_message_attr_find(resp, STUN_ATTR_LIFETIME);

	allocate = turn_agent_allocation_find_by_address(&stun->turnclients, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
	if (!allocate)
		return (!attr || 0 == attr->v.u32) ? 0 : -1;

	if (!attr || 0 == attr->v.u32)
	{
		turn_agent_allocation_remove(&stun->turnclients, allocate);
		turn_allocation_destroy(&allocate);
		return 0;
	}

	allocate->expire = system_clock() + (attr ? attr->v.u32 : TURN_LIFETIME) * 1000;
	return 0;
}

//int turn_agent_create_permission(stun_request_t* req, const struct sockaddr_storage* peers[], int n)
int turn_agent_create_permission(stun_request_t* req, const struct sockaddr* peer)
{
	struct stun_message_t* msg;
	msg = &req->msg;

	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_CREATE_PERMISSION);
	
	if (AF_INET != peer->sa_family && AF_INET6 != peer->sa_family)
		return -1;
	stun_message_add_address(msg, STUN_ATTR_XOR_PEER_ADDRESS, peer);
	
	stun_message_add_credentials(msg, &req->auth);
	stun_message_add_fingerprint(msg);
	return stun_request_send(req->stun, req);
}

static int turn_client_add_permission(void* param, const struct stun_attr_t* attr)
{
	struct turn_allocation_t* allocate;
	allocate = (struct turn_allocation_t*)param;
	return turn_allocation_add_permission(allocate, (const struct sockaddr*)&attr->v.addr);
}

int turn_client_create_permission_onresponse(struct stun_agent_t* stun, struct stun_request_t* req, const struct stun_message_t* resp)
{
	struct turn_allocation_t* allocate;
	allocate = turn_agent_allocation_find_by_address(&stun->turnclients, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
	if (!allocate) return -1;

	(void)resp;
	return stun_message_attr_list(&req->msg, STUN_ATTR_XOR_PEER_ADDRESS, turn_client_add_permission, allocate);
}

int turn_agent_channel_bind(stun_request_t* req, const struct sockaddr* peer, uint16_t channel)
{
	struct stun_message_t* msg;
	msg = &req->msg;

	if (AF_INET != peer->sa_family && AF_INET6 != peer->sa_family)
		return -1;
	if (channel < 0x4000 || channel > 0x7FFE)
		return -1;

	msg->header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_CHANNEL_BIND);
	stun_message_add_address(msg, STUN_ATTR_XOR_PEER_ADDRESS, peer);
	stun_message_add_uint32(msg, STUN_ATTR_CHANNEL_NUMBER, channel << 16);

	stun_message_add_credentials(msg, &req->auth);
	stun_message_add_fingerprint(msg);
	return stun_request_send(req->stun, req);
}

int turn_client_channel_bind_onresponse(struct stun_agent_t* stun, struct stun_request_t* req, const struct stun_message_t* resp)
{
	int r;
	const struct stun_attr_t* peer;
	const struct stun_attr_t* channel;
	struct turn_allocation_t* allocate;
	
	(void)resp;
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

static int turn_client_send_channel_data(struct stun_agent_t* turn, const struct turn_allocation_t* allocate, const struct turn_channel_t* channel, const void* data, int bytes)
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

int turn_agent_send(stun_agent_t* stun, const struct sockaddr* relay, const struct sockaddr* peer, const void* data, int bytes)
{
	struct stun_message_t msg;
    struct turn_allocation_t* allocate;
    const struct turn_channel_t* channel;

    allocate = turn_agent_allocation_find_by_relay(&stun->turnclients, relay);
	if (!allocate)
		return -1;
    
	channel = turn_allocation_find_channel_by_peer(allocate, peer);
	if(channel)
		return turn_client_send_channel_data(stun, allocate, channel, data, bytes);

	memset(&msg, 0, sizeof(struct stun_message_t));
	msg.header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_INDICATION, STUN_METHOD_SEND);
	msg.header.length = 0;
	msg.header.cookie = STUN_MAGIC_COOKIE;
	stun_transaction_id(msg.header.tid, sizeof(msg.header.tid));
	stun_message_add_string(&msg, STUN_ATTR_SOFTWARE, STUN_SOFTWARE);

	//stun_message_add_flag(&msg, STUN_ATTR_DONT_FRAGMENT);
	stun_message_add_address(&msg, STUN_ATTR_XOR_PEER_ADDRESS, peer);
	stun_message_add_data(&msg, STUN_ATTR_DATA, data, bytes);

	stun_message_add_credentials(&msg, &allocate->auth);
	stun_message_add_fingerprint(&msg);

	// STUN indications are not retransmitted
	assert(turn_allocation_find_permission(allocate, peer));
	return stun_message_send(stun, &msg, allocate->addr.protocol, &allocate->addr.host, &allocate->addr.peer, NULL);
}

int turn_client_ondata(struct stun_agent_t* turn, const struct stun_request_t* req)
{
	const struct stun_attr_t* data;
	const struct stun_attr_t* peer;
	struct turn_allocation_t* allocate;
	
	peer = stun_message_attr_find(&req->msg, STUN_ATTR_XOR_PEER_ADDRESS);
	data = stun_message_attr_find(&req->msg, STUN_ATTR_DATA);
	if (!peer || !data) return 0; // discard

	allocate = turn_agent_allocation_find_by_address(&turn->turnclients, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
	if (!allocate) return 0; // discard

	// TODO: check allocation lifetime ???
	assert(req->addr.protocol == allocate->addr.protocol);
	assert(turn_allocation_find_permission(allocate, (const struct sockaddr*)&peer->v.addr));
	assert(0 == socket_addr_compare((const struct sockaddr*)&req->addr.peer, (const struct sockaddr*)&allocate->addr.peer));
	//turn->handler.ondata(turn->param, allocate->addr.protocol, (const struct sockaddr*)&allocate->addr.host, (const struct sockaddr*)&peer->v.addr, data->v.ptr, data->length);
	return stun_agent_input2(turn, allocate->addr.protocol, (const struct sockaddr*)&allocate->addr.host, (const struct sockaddr*)&peer->v.addr, (const struct sockaddr*)&allocate->addr.relay, data->v.ptr, data->length);
}

// ChannelData from server
int turn_client_onchannel_data(struct stun_agent_t* turn, struct turn_allocation_t* allocate, const uint8_t* data, int bytes)
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
	{
		//turn->handler.ondata(turn->param, allocate->addr.protocol, (const struct sockaddr*)&allocate->addr.host, (const struct sockaddr*)&channel->addr, (const uint8_t*)data + 4, length);
		return stun_agent_input2(turn, allocate->addr.protocol, (const struct sockaddr*)&allocate->addr.host, (const struct sockaddr*)&channel->addr, (const struct sockaddr*)&allocate->addr.relay, (const uint8_t*)data + 4, length);
	}
	
	return 0;
}
