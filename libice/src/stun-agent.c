#include "stun-internal.h"
#include "turn-internal.h"
#include "stun-message.h"
#include "byte-order.h"
#include "sockutil.h"
#include "list.h"
#include <stdlib.h>
#include <assert.h>

struct stun_agent_t* stun_agent_create(int rfc, struct stun_agent_handler_t* handler, void* param)
{
	struct stun_agent_t* stun;
	stun = (struct stun_agent_t*)calloc(1, sizeof(*stun));
	if (stun)
	{
		stun->rfc = rfc;
		stun->auth_term = 0; // disable bind request auth check
		LIST_INIT_HEAD(&stun->requests);
		LIST_INIT_HEAD(&stun->turnclients);
		LIST_INIT_HEAD(&stun->turnservers);
        LIST_INIT_HEAD(&stun->turnreserved);
		locker_create(&stun->locker);
		memcpy(&stun->handler, handler, sizeof(stun->handler));
		stun->param = param;
	}
	return stun;
}

int stun_agent_destroy(stun_agent_t** pp)
{
	stun_agent_t* stun;
	struct stun_request_t* req;
	struct turn_allocation_t* allocate;
	struct list_head* pos, *next;

	if (!pp || !*pp)
		return 0;

	stun = *pp;
	locker_lock(&stun->locker);
	list_for_each_safe(pos, next, &stun->requests)
	{
		req = list_entry(pos, struct stun_request_t, link);
		stun_request_addref(req);
		stun_request_prepare(req);
		stun_request_release(req);
	}
	locker_unlock(&stun->locker);

	list_for_each_safe(pos, next, &stun->turnclients)
	{
		allocate = list_entry(pos, struct turn_allocation_t, link);
		turn_allocation_destroy(&allocate);
	}

	list_for_each_safe(pos, next, &stun->turnservers)
	{
		allocate = list_entry(pos, struct turn_allocation_t, link);
		turn_allocation_destroy(&allocate);
	}
    
    list_for_each_safe(pos, next, &stun->turnreserved)
    {
        allocate = list_entry(pos, struct turn_allocation_t, link);
        turn_allocation_destroy(&allocate);
    }

	locker_destroy(&stun->locker);
	free(stun);
	*pp = NULL;
	return 0;
}

int stun_agent_insert(struct stun_agent_t* stun, stun_request_t* req)
{
	stun_request_addref(req);
	locker_lock(&stun->locker);
	if (req->link.next || req->link.prev)
	{
		assert(0);
		locker_unlock(&stun->locker);
		stun_request_release(req);
		return -1;
	}
	list_insert_after(&req->link, &stun->requests);
	locker_unlock(&stun->locker);
	return 0;
}

int stun_agent_remove(struct stun_agent_t* stun, stun_request_t* req)
{
	locker_lock(&stun->locker);
	if (req->link.next == NULL || req->link.prev == NULL)
	{
		assert(0);
		locker_unlock(&stun->locker);
		return -1;
	}
	list_remove(&req->link);
	locker_unlock(&stun->locker);
	stun_request_release(req);
	return 0;
}

static struct stun_request_t* stun_agent_fetch(struct stun_agent_t* stun, const struct stun_message_t* msg)
{
	struct list_head *ptr, *next;
	struct stun_request_t* entry;

	locker_lock(&stun->locker);
	list_for_each_safe(ptr, next, &stun->requests)
	{
		entry = list_entry(ptr, struct stun_request_t, link);
		if (0 == memcmp(entry->msg.header.tid, msg->header.tid, sizeof(msg->header.tid)))
		{
			stun_request_addref(entry);
			locker_unlock(&stun->locker);
			return entry;
		}
	}
	locker_unlock(&stun->locker);
	return NULL;
}

int stun_message_send(struct stun_agent_t* stun, struct stun_message_t* msg, int protocol, const struct sockaddr_storage* local, const struct sockaddr_storage* remote, const struct sockaddr_storage* relay)
{
	int r, bytes;
	uint8_t data[1600];

	assert(stun && msg);
	r = stun_message_write(data, sizeof(data), msg);
	if (0 != r) return r;

	bytes = msg->header.length + STUN_HEADER_SIZE;
	if (relay && (AF_INET == relay->ss_family || AF_INET6 == relay->ss_family))
		return turn_agent_send(stun, (const struct sockaddr*)relay, (const struct sockaddr*)remote, data, bytes);
	else
		return stun->handler.send(stun->param, protocol, (const struct sockaddr*)local, (const struct sockaddr*)remote, data, bytes);
}

static int stun_agent_onrequest(struct stun_agent_t* stun, struct stun_request_t* req)
{
	struct stun_response_t* resp;
	resp = stun_response_create(stun, req);
	if (NULL == resp)
		return -1; // -ENOMEM

	switch (STUN_MESSAGE_METHOD(req->msg.header.msgtype))
	{
	case STUN_METHOD_BIND:
		return stun_server_onbind(stun, req, resp);

	case STUN_METHOD_SHARED_SECRET:
		return stun_server_onshared_secret(stun, req, resp);

	case STUN_METHOD_ALLOCATE:
		return turn_server_onallocate(stun, req, resp);

	case STUN_METHOD_REFRESH:
		return turn_server_onrefresh(stun, req, resp);

	case STUN_METHOD_CREATE_PERMISSION:
		return turn_server_oncreate_permission(stun, req, resp);

	case STUN_METHOD_CHANNEL_BIND:
		return turn_server_onchannel_bind(stun, req, resp);

	default:
		assert(0);
		return -1;
	}
}

static int stun_agent_onindication(stun_agent_t* stun, const struct stun_request_t* req)
{
	switch (STUN_MESSAGE_METHOD(req->msg.header.msgtype))
	{
    case STUN_METHOD_BIND: // client -> stun server, refresh
        return stun_server_onbindindication(stun, req);
            
	case STUN_METHOD_SEND: // client -> turn server
		return turn_server_onsend(stun, req);

	case STUN_METHOD_DATA: // turn server -> client
		return turn_client_ondata(stun, req);

	default:
		assert(0);
		return -1;
	}
}

static int stun_agent_onsuccess(stun_agent_t* stun, const struct stun_message_t* resp, struct stun_request_t* req)
{
	int r;

	r = 0;
	switch (STUN_MESSAGE_METHOD(req->msg.header.msgtype))
	{
	case STUN_METHOD_BIND:
		// filled in stun_agent_parse_attr
		break;

	case STUN_METHOD_SHARED_SECRET:
		// filled in stun_agent_parse_attr
		break;

	case STUN_METHOD_ALLOCATE:
		r = turn_client_allocate_onresponse(stun, req, resp);
		break;

	case STUN_METHOD_REFRESH:
		r = turn_client_refresh_onresponse(stun, req, resp);
		break;

	case STUN_METHOD_CREATE_PERMISSION:
		r = turn_client_create_permission_onresponse(stun, req, resp);
		break;

	case STUN_METHOD_CHANNEL_BIND:
		r = turn_client_channel_bind_onresponse(stun, req, resp);
		break;

	default:
		assert(0);
		return -1;
	}

	return req->handler(req->param, req, r, 0 == r ? "OK" : "Unknown Error");
}

static int stun_agent_onfailure(stun_agent_t* stun, const struct stun_message_t* resp, struct stun_request_t* req)
{
	struct stun_request_t* req2;
	const struct stun_attr_t* error;
	error = stun_message_attr_find(resp, STUN_ATTR_ERROR_CODE);
	if (error)
	{
		// fixed: check nonce value to avoid recurse call
		if (STUN_CREDENTIAL_LONG_TERM == req->auth.credential && (401 == error->v.errcode.code || 438 == error->v.errcode.code) && 0!=req->auth.nonce[0] && req->authtimes < 1)
		{
			// 1. If the response is an error response with an error code of 401 (Unauthorized),
			//    the client SHOULD retry the request with a new transaction.
			// 2. If the response is an error response with an error code of 438 (Stale Nonce),
			//    the client MUST retry the request, using the new NONCE supplied in the 438 (Stale Nonce) response.
			req2 = stun_request_create(stun, req->rfc, req->handler, req->param);
			req2->authtimes = req->authtimes + 1;
			memcpy(&req2->addr, &req->addr, sizeof(struct stun_address_t));
			memcpy(&req2->msg, &req->msg, sizeof(struct stun_message_t));
			stun_request_setauth(req2, req->auth.credential, req->auth.usr, req->auth.pwd, req->auth.realm, req->auth.nonce);
			stun_message_add_credentials(&req2->msg, &req2->auth);
			stun_message_add_fingerprint(&req2->msg);
			if (0 == stun_request_send(stun, req2))
				return 0;
		}

		return req->handler(req->param, req, error->v.errcode.code, error->v.errcode.reason_phrase);
	}
	else
	{
		return req->handler(req->param, req, 500, "Server internal error");
	}
}

static int stun_server_response_unknown_attribute(stun_agent_t* stun, struct stun_request_t* req)
{
	int i, j;
	uint16_t unknowns[STUN_ATTR_N];
	struct stun_message_t msg;

	for (j = i = 0; i < req->msg.nattrs; i++)
	{
		if(-1 != req->msg.attrs[i].unknown)
			continue;
		unknowns[j++] = req->msg.attrs[i].type;
	}
	assert(j > 0);

	memset(&msg, 0, sizeof(struct stun_message_t));
	memcpy(&msg.header, &req->msg.header, sizeof(struct stun_header_t));
    msg.header.length = 0;
	msg.header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_FAILURE_RESPONSE, STUN_MESSAGE_METHOD(req->msg.header.msgtype));

	stun_message_add_error(&msg, 420, "Unknown Attribute");
	stun_message_add_data(&msg, STUN_ATTR_UNKNOWN_ATTRIBUTES, unknowns, j * sizeof(unknowns[0]));
	stun_message_add_credentials(&msg, &req->auth);
	stun_message_add_fingerprint(&msg);
	assert(0 == req->addr.relay.ss_family); // can't be relay
	return stun_message_send(stun, &msg, req->addr.protocol, &req->addr.host, &req->addr.peer, NULL);
}

static int stun_agent_parse_attr(stun_agent_t* stun, struct stun_request_t* req, const struct stun_message_t* msg)
{
	int i;
	struct stun_address_t* addr;
	struct stun_credential_t* auth;
    
	addr = &req->addr;
	auth = &req->auth;

	for (i = 0; i < msg->nattrs; i++)
	{
		switch (msg->attrs[i].type)
		{
		case STUN_ATTR_NONCE:
			snprintf(auth->nonce, sizeof(auth->nonce), "%.*s", (int)msg->attrs[i].length, (const char*)msg->attrs[i].v.ptr);
			break;

		case STUN_ATTR_REALM:
			snprintf(auth->realm, sizeof(auth->realm), "%.*s", (int)msg->attrs[i].length, (const char*)msg->attrs[i].v.ptr);
			break;

		case STUN_ATTR_USERNAME:
			snprintf(auth->usr, sizeof(auth->usr), "%.*s", (int)msg->attrs[i].length, (const char*)msg->attrs[i].v.ptr);
			break;

		case STUN_ATTR_PASSWORD:
			snprintf(auth->pwd, sizeof(auth->pwd), "%.*s", (int)msg->attrs[i].length, (const char*)msg->attrs[i].v.ptr);
			break;

		case STUN_ATTR_SOURCE_ADDRESS:
			// server response address, deprecated
			break;

        case STUN_ATTR_MAPPED_ADDRESS:
            // don't overwrite STUN_ATTR_XOR_MAPPED_ADDRESS
            if(AF_INET != addr->reflexive.ss_family && AF_INET6 != addr->reflexive.ss_family)
                memcpy(&addr->reflexive, &msg->attrs[i].v.addr, sizeof(struct sockaddr_storage));
            break;

		case STUN_ATTR_XOR_MAPPED_ADDRESS:
			memcpy(&addr->reflexive, &msg->attrs[i].v.addr, sizeof(struct sockaddr_storage));
			break;

		case STUN_ATTR_XOR_RELAYED_ADDRESS:
			memcpy(&addr->relay, &msg->attrs[i].v.addr, sizeof(struct sockaddr_storage));
			break;

		default:
			if (msg->attrs[i].type < 0x7fff && -1 == msg->attrs[i].unknown)
			{
				if(STUN_MESSAGE_CLASS(req->msg.header.msgtype) == STUN_METHOD_CLASS_REQUEST || STUN_MESSAGE_CLASS(req->msg.header.msgtype) == STUN_METHOD_CLASS_INDICATION)
					stun_server_response_unknown_attribute(stun, req);
				return -1; // unknown attributes
			}
		}
	}

	return 0;
}

static int stun_agent_onresponse(stun_agent_t* stun, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes, const struct stun_message_t* msg)
{
	int r;
	struct stun_request_t *req;

	req = stun_agent_fetch(stun, msg);
	if (!req)
		return 0; //unknown request, ignore

	if (req->addr.protocol != protocol || (local && 0 != socket_addr_compare((const struct sockaddr*)&req->addr.host, local)) || 0 != socket_addr_compare((const struct sockaddr*)&req->addr.peer, remote))
	{
		assert(0);
		stun_request_release(req);
		return -1;
	}

	if (0 != stun_agent_parse_attr(stun, req, msg))
	{
		assert(0);
		stun_request_release(req);
		return 0; // discard
	}
	
	r = -1;
	switch (STUN_MESSAGE_CLASS(msg->header.msgtype))
	{
	case STUN_METHOD_CLASS_SUCCESS_RESPONSE:
		if (0 == stun_request_prepare(req) && 0 == stun_agent_response_auth_check(stun, msg, req, data, bytes))
			r = stun_agent_onsuccess(stun, msg, req);
		break;

	case STUN_METHOD_CLASS_FAILURE_RESPONSE:
		if (0 == stun_request_prepare(req))
			r = stun_agent_onfailure(stun, msg, req);
		break;

	default:
		assert(0);
	}

	stun_request_release(req);
	return r;
}

int stun_agent_input(stun_agent_t* stun, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	return stun_agent_input2(stun, protocol, local, remote, NULL, data, bytes);
}

int stun_agent_input2(stun_agent_t* stun, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relayed, const void* data, int bytes)
{
	int r;
	struct stun_request_t req;
	struct turn_allocation_t* allocate;

	if (local)
	{
		// 1. turn server receive relay data ?
		allocate = turn_agent_allocation_find_by_relay(&stun->turnservers, local);
		if (allocate)
			return turn_server_relay(stun, allocate, remote, data, bytes);
	}
    
	// 2. turn server/client receive channel data ? (channel range: 0x4000 ~ 0x7FFF)
	//    RFC5766 11. Channels (p37), 0b00-STUN-formatted message, 0b01-ChannelData
	if (bytes > 0 && 0x40 == (0xC0 & ((const uint8_t*)data)[0]) && local && remote)
	{
		allocate = turn_agent_allocation_find_by_address(&stun->turnclients, local, remote);
		if (allocate)
		{
			assert(allocate->addr.protocol == protocol);
			return turn_client_onchannel_data(stun, allocate, (const uint8_t*)data, bytes);
		}
		else
		{
			allocate = turn_agent_allocation_find_by_address(&stun->turnservers, local, remote);
			if (allocate)
				return turn_server_onchannel_data(stun, allocate, (const uint8_t*)data, bytes);
		}

		assert(0); // allocation expired ?
		return 0;
	}

	if (bytes > 0 && 0 != (0xC0 & ((const uint8_t*)data)[0]))
	{
		stun->handler.ondata(stun->param, protocol, local, remote, data, bytes);
		return 0;
	}

	// 3. stun/turn request/response
	memset(&req, 0, sizeof(req));
	r = stun_message_read(&req.msg, data, bytes);
	if (0 != r)
		return r;

	req.stun = stun;
	switch (STUN_MESSAGE_CLASS(req.msg.header.msgtype))
	{
	case STUN_METHOD_CLASS_REQUEST:
		stun_request_setaddr(&req, protocol, local, remote, relayed);
		if (0 != stun_agent_parse_attr(stun, &req, &req.msg))
			return 0; // discard

		if(0 == stun_agent_request_auth_check(stun, &req, data, bytes))
			r = stun_agent_onrequest(stun, &req);
		break;

	case STUN_METHOD_CLASS_INDICATION:
		stun_request_setaddr(&req, protocol, local, remote, relayed);
		if (0 != stun_agent_parse_attr(stun, &req, &req.msg))
			return 0; // discard

		//if (0 == stun_agent_request_auth_check(stun, &req, data, bytes))
			r = stun_agent_onindication(stun, &req);
		break;

	case STUN_METHOD_CLASS_SUCCESS_RESPONSE:
	case STUN_METHOD_CLASS_FAILURE_RESPONSE:
		r = stun_agent_onresponse(stun, protocol, local, remote, data, bytes, &req.msg);
		break;

	default:
		assert(0);
		r = -1;
	}

	return r;
}
