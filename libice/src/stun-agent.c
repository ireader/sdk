#include "stun-agent.h"
#include "stun-message.h"
#include "stun-internal.h"
#include "turn-internal.h"
#include "byte-order.h"
#include "sockutil.h"
#include "list.h"
#include <stdlib.h>

struct stun_agent_t* stun_agent_create(int rfc, struct stun_agent_handler_t* handler, void* param)
{
	struct stun_agent_t* stun;
	stun = (struct stun_agent_t*)calloc(1, sizeof(*stun));
	if (stun)
	{
		stun->rfc = rfc;
		stun->auth_term = STUN_RFC_3489 == rfc ? 0 : 1;
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
	list_for_each_safe(pos, next, &stun->requests)
	{
		req = list_entry(pos, struct stun_request_t, link);
		free(req);
	}

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

struct stun_request_t* stun_agent_find(struct stun_agent_t* stun, const struct stun_message_t* msg)
{
	struct list_head *ptr, *next;
	struct stun_request_t* entry;
	list_for_each_safe(ptr, next, &stun->requests)
	{
		entry = list_entry(ptr, struct stun_request_t, link);
		if (0 == memcmp(entry->msg.header.tid, msg->header.tid, sizeof(msg->header.tid)))
			return entry;
	}
	return NULL;
}

int stun_agent_insert(struct stun_agent_t* stun, stun_request_t* req)
{
	stun_request_addref(req);
	locker_lock(&stun->locker);
	list_insert_after(&req->link, &stun->requests);
	locker_unlock(&stun->locker);
	return 0;
}

int stun_agent_remove(struct stun_agent_t* stun, stun_request_t* req)
{
	locker_lock(&stun->locker);
	list_remove(&req->link);
	locker_unlock(&stun->locker);
	stun_request_release(req);
	return 0;
}

int stun_message_send(struct stun_agent_t* stun, struct stun_message_t* msg, int protocol, const struct sockaddr_storage* local, const struct sockaddr_storage* remote)
{
	int r, bytes;
	uint8_t data[1600];

	assert(stun && msg);
	r = stun_message_write(data, sizeof(data), msg);
	if (0 != r) return r;

	bytes = msg->header.length + STUN_HEADER_SIZE;
	return stun->handler.send(stun->param, protocol, (const struct sockaddr*)local, (const struct sockaddr*)remote, data, bytes);
}

// rfc5389 7.2.1. Sending over UDP (p13)
static void stun_request_ontimer(void* param)
{
	stun_request_t* req;
	req = (stun_request_t*)param;
	req->timeout = req->timeout * 2 + STUN_RETRANSMISSION_INTERVAL_MIN;
	if (req->timeout <= STUN_RETRANSMISSION_INTERVAL_MAX)
	{
		// 11000 = STUN_TIMEOUT - (500 + 1500 + 3500 + 7500 + 15500)
		req->timer = stun_timer_start(req->timeout <= STUN_RETRANSMISSION_INTERVAL_MAX ? req->timeout : 11000, stun_request_ontimer, req);
		if(req->timer)
			return;
	}

	req->handler(req->param, req, 408, "Request Timeout");
	stun_agent_remove(req->stun, req);
}

int stun_request_send(struct stun_agent_t* stun, struct stun_request_t* req)
{
	int r;
	assert(stun && req);
    
	stun_agent_insert(stun, req);
	req->timeout = STUN_RETRANSMISSION_INTERVAL_MIN;
	req->timer = stun_timer_start(STUN_RETRANSMISSION_INTERVAL_MIN, stun_request_ontimer, req);

	r = stun_message_send(stun, &req->msg, req->addr.protocol, &req->addr.host, &req->addr.peer);
	if (0 != r)
	{
		stun_timer_stop(req->timer);
		stun_agent_remove(stun, req);
	}
	
	return r;
}

int stun_response_send(struct stun_agent_t* stun, struct stun_response_t* resp)
{
	int r;
	r = stun_message_send(stun, &resp->msg, resp->addr.protocol, &resp->addr.host, &resp->addr.peer);
	stun_response_destroy(&resp);
	return r;
}

static int stun_agent_onrequest(struct stun_agent_t* stun, struct stun_request_t* req)
{
	struct stun_response_t* resp;
	resp = stun_response_create(req);
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

static int stun_agent_onsuccess(stun_agent_t* stun, const struct stun_request_t* resp)
{
	int r;
	struct stun_request_t* req;
	req = stun_agent_find(stun, &resp->msg);
	if (!req)
		return 0; // discard

	// fill reflexive/relayed address
	memcpy(&req->addr.reflexive, &resp->addr.reflexive, sizeof(struct sockaddr_storage));
    
	r = 0;
	switch (STUN_MESSAGE_METHOD(req->msg.header.msgtype))
	{
	case STUN_METHOD_BIND:
		break;

	case STUN_METHOD_SHARED_SECRET:
		memcpy(&req->auth, &resp->auth, sizeof(struct stun_credential_t));
		break;

	case STUN_METHOD_ALLOCATE:
		r = turn_client_allocate_onresponse(req, resp);
		break;

	case STUN_METHOD_REFRESH:
		r = turn_client_refresh_onresponse(req, resp);
		break;

	case STUN_METHOD_CREATE_PERMISSION:
		r = turn_client_create_permission_onresponse(req, resp);
		break;

	case STUN_METHOD_CHANNEL_BIND:
		r = turn_client_channel_bind_onresponse(req, resp);
		break;

	default:
		assert(0);
		return -1;
	}

	if(0 == stun_timer_stop(req->timer))
		r = req->handler(req->param, req, 0, "OK");

	stun_agent_remove(stun, req);
	return r;
}

static int stun_agent_onfailure(stun_agent_t* stun, const struct stun_request_t* resp)
{
	int r;
	struct stun_request_t* req;
	const struct stun_attr_t* error;
	req = stun_agent_find(stun, &resp->msg);
	if (!req)
		return 0; // discard

	r = 0;
	if (0 == stun_timer_stop(req->timer))
	{
        req->timer = NULL;
		error = stun_message_attr_find(&resp->msg, STUN_ATTR_ERROR_CODE);
		if (error)
        {
            if (STUN_CREDENTIAL_LONG_TERM == req->auth.credential && 401 == error->v.errcode.code)
            {
                // If the response is an error response with an error code of 401 (Unauthorized),
                // the client SHOULD retry the request with a new transaction.
                stun_transaction_id(req->msg.header.tid, sizeof(req->msg.header.tid));
                memcpy(req->auth.realm, resp->auth.realm, sizeof(req->auth.realm));
                memcpy(req->auth.nonce, resp->auth.nonce, sizeof(req->auth.nonce));
                stun_message_add_credentials(&req->msg, &req->auth);
                stun_message_add_fingerprint(&req->msg);
//                req2 = stun_request_create(stun, req->rfc, req->handler, req->param);
//                stun_request_setaddr(req2, req->addr.protocol, (const struct sockaddr*)&req->addr.host, (const struct sockaddr*)&req->addr.peer);
//                stun_request_setauth(req2, req->auth.credential, req->auth.usr, req->auth.pwd, resp->auth.realm, resp->auth.nonce);
                if(0 == stun_request_send(req->stun, req))
                    return 0; // TODO: timer/reference
            }
            else if (STUN_CREDENTIAL_LONG_TERM == req->auth.credential && 438 == error->v.errcode.code)
            {
                // If the response is an error response with an error code of 438 (Stale Nonce),
                // the client MUST retry the request, using the new NONCE supplied in the 438 (Stale Nonce) response.
                stun_transaction_id(req->msg.header.tid, sizeof(req->msg.header.tid));
                memcpy(req->auth.nonce, resp->auth.nonce, sizeof(req->auth.nonce));
                stun_message_add_credentials(&req->msg, &req->auth);
                stun_message_add_fingerprint(&req->msg);
                if(0 == stun_request_send(req->stun, req))
                    return 0; // TODO: timer/reference
            }
            
			r = req->handler(req->param, req, error->v.errcode.code, error->v.errcode.reason_phrase);
        }
		else
        {
			r = req->handler(req->param, req, 500, "Server internal error");
        }
	}

	stun_agent_remove(stun, req);
	return r;
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
	return stun_message_send(stun, &msg, req->addr.protocol, &req->addr.host, &req->addr.peer);
}

static int stun_agent_parse_attr(stun_agent_t* stun, struct stun_request_t* req)
{
	int i;
	struct stun_message_t* msg;
	struct stun_address_t* addr;
	struct stun_credential_t* auth;
    
	msg = &req->msg;
	addr = &req->addr;
	auth = &req->auth;

	for (i = 0; i < msg->nattrs; i++)
	{
		switch (msg->attrs[i].type)
		{
		case STUN_ATTR_NONCE:
			snprintf(auth->nonce, sizeof(auth->nonce), "%.*s", msg->attrs[i].length, (const char*)msg->attrs[i].v.ptr);
			break;

		case STUN_ATTR_REALM:
			snprintf(auth->realm, sizeof(auth->realm), "%.*s", msg->attrs[i].length, (const char*)msg->attrs[i].v.ptr);
			break;

		case STUN_ATTR_USERNAME:
			snprintf(auth->usr, sizeof(auth->usr), "%.*s", msg->attrs[i].length, (const char*)msg->attrs[i].v.ptr);
			break;

		case STUN_ATTR_PASSWORD:
			snprintf(auth->pwd, sizeof(auth->pwd), "%.*s", msg->attrs[i].length, (const char*)msg->attrs[i].v.ptr);
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

int stun_agent_input(stun_agent_t* stun, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	int r;
	struct stun_request_t req;
	struct stun_message_t *msg;
	struct turn_allocation_t* allocate;

    if (local)
    {
        allocate = turn_agent_allocation_find_by_relay(&stun->turnservers, local);
        if (allocate)
            return turn_server_relay(stun, allocate, remote, data, bytes);
    }
    
	// 0x4000 ~ 0x7FFFF
	if (bytes > 0 && 0x40 == (0xC0 & ((const uint8_t*)data)[0]) && local && remote)
	{
		allocate = turn_agent_allocation_find_by_address(&stun->turnclients, local, remote);
		if (allocate)
		{
			return turn_client_onchannel_data(stun, allocate, (const uint8_t*)data, bytes);
		}
		else
		{
			allocate = turn_agent_allocation_find_by_address(&stun->turnservers, local, remote);
			if (allocate)
				return turn_server_onchannel_data(stun, allocate, (const uint8_t*)data, bytes);
		}

		return 0;
	}

	memset(&req, 0, sizeof(req));
	req.stun = stun;
	msg = &req.msg;
	r = stun_message_read(msg, data, bytes);
	if (0 != r)
		return r;
	
	stun_request_setaddr(&req, protocol, local, remote);

	if (0 != stun_agent_parse_attr(stun, &req) || 0 != stun_agent_auth(stun, &req, data, bytes))
		return 0; // discard

	switch (STUN_MESSAGE_CLASS(req.msg.header.msgtype))
	{
	case STUN_METHOD_CLASS_REQUEST:
		r = stun_agent_onrequest(stun, &req);
		break;

	case STUN_METHOD_CLASS_INDICATION:
		r = stun_agent_onindication(stun, &req);
		break;

	case STUN_METHOD_CLASS_SUCCESS_RESPONSE:
		r = stun_agent_onsuccess(stun, &req);
		break;

	case STUN_METHOD_CLASS_FAILURE_RESPONSE:
		r = stun_agent_onfailure(stun, &req);
		break;

	default:
		assert(0);
		r = -1;
	}

	return r;
}
