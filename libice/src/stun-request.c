#include "stun-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

enum { STUN_REQUEST_INIT = 0, STUN_REQUEST_RUNNING, STUN_REQUEST_DONE };

stun_request_t* stun_request_create(stun_agent_t* stun, int rfc, stun_request_handler handler, void* param)
{
	stun_request_t* req;
	struct stun_message_t* msg;
	req = (stun_request_t*)calloc(1, sizeof(stun_request_t));
	if (!req) return NULL;

	req->ref = 0;
	req->rfc = rfc;
	req->stun = stun;
	req->timeout = STUN_TIMEOUT;
	req->interval = STUN_RETRANSMISSION_INTERVAL_MIN;
	req->state = STUN_REQUEST_INIT;
	req->param = param;
	req->handler = handler;
	//LIST_INIT_HEAD(&req->link);
	locker_create(&req->locker);

	msg = &req->msg;
//	memset(msg, 0, sizeof(struct stun_message_t));
//	msg->header.msgtype = STUN_METHOD_BIND | STUN_METHOD_CLASS_REQUEST;
	msg->header.length = 0;
	msg->header.cookie = STUN_MAGIC_COOKIE;
	stun_transaction_id(msg->header.tid, sizeof(msg->header.tid));

	stun_message_add_string(msg, STUN_ATTR_SOFTWARE, STUN_SOFTWARE);
	stun_agent_insert(stun, req);
	return req;
}

int stun_request_prepare(struct stun_request_t* req)
{
	// lock for handler callback
	locker_lock(&req->locker);
	if (STUN_REQUEST_DONE == req->state)
	{
		// don't need release
		locker_unlock(&req->locker);
		return -1;
	}

	assert(req->ref >= 2);
	req->state = STUN_REQUEST_DONE; // cancel
	stun_agent_remove(req->stun, req); // delete link
	if (req->timer && 0 == stun_timer_stop(req->timer))
	{
		assert(req->ref == 2);
		stun_request_release(req); // for timer
	}

	locker_unlock(&req->locker);
	return 0;
}

int stun_request_addref(struct stun_request_t* req)
{
	return atomic_increment32(&req->ref);
}

int stun_request_release(struct stun_request_t* req)
{
	if (0 == atomic_decrement32(&req->ref))
	{
		assert(STUN_REQUEST_DONE == req->state);
		assert(req->link.next == NULL);
		locker_destroy(&req->locker);
		free(req);
	}
	return 0;
}

int stun_request_setaddr(stun_request_t* req, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relayed)
{
	if (!remote || (STUN_PROTOCOL_UDP != protocol && STUN_PROTOCOL_TCP != protocol && STUN_PROTOCOL_TLS != protocol))
	{
		assert(0);
		return -1;
	}

	memset(&req->addr, 0, sizeof(req->addr));
	req->addr.protocol = protocol;
	memcpy(&req->addr.host, local, local ? socket_addr_len(local) : 0);
	memcpy(&req->addr.peer, remote, remote ? socket_addr_len(remote) : 0);
	memcpy(&req->addr.relay, relayed, relayed ? socket_addr_len(relayed) : 0);
	return 0;
}

int stun_request_getaddr(const stun_request_t* req, int* protocol, struct sockaddr_storage* local, struct sockaddr_storage* remote, struct sockaddr_storage* reflexive, struct sockaddr_storage* relay)
{
	if(protocol) *protocol = req->addr.protocol;
	if (local) memcpy(local, &req->addr.host, sizeof(struct sockaddr_storage));
	if (remote) memcpy(remote, &req->addr.peer, sizeof(struct sockaddr_storage));
	if (reflexive) memcpy(reflexive, &req->addr.reflexive, sizeof(struct sockaddr_storage));
	if (relay) memcpy(relay, &req->addr.relay, sizeof(struct sockaddr_storage));
	return 0;
}

int stun_request_setauth(stun_request_t* req, int credential, const char* usr, const char* pwd, const char* realm, const char* nonce)
{
	return stun_credential_setauth(&req->auth, credential, usr, pwd, realm, nonce);
}

int stun_request_getauth(const stun_request_t* req, char usr[512], char pwd[512], char realm[128], char nonce[128])
{
	if (usr) snprintf(usr, 512, "%s", req->auth.usr);
	if (pwd) snprintf(pwd, 512, "%s", req->auth.pwd);
    if (realm) snprintf(realm, 128, "%s", req->auth.realm);
    if (nonce) snprintf(nonce, 128, "%s", req->auth.nonce);
	return 0;
}

const stun_message_t* stun_request_getmessage(const stun_request_t* req)
{
	return &req->msg;
}

void stun_request_settimeout(stun_request_t* req, int timeout)
{
	req->timeout = timeout;
}

// rfc5389 7.2.1. Sending over UDP (p13)
static void stun_request_ontimer(void* param)
{
	stun_request_t* req;
	req = (stun_request_t*)param;
	
	locker_lock(&req->locker);
	if (STUN_REQUEST_DONE != req->state)
	{
		if (req->elapsed < req->timeout && 0 == stun_message_send(req->stun, &req->msg, req->addr.protocol, &req->addr.host, &req->addr.peer, &req->addr.relay))
		{
			req->interval = req->interval < STUN_RETRANSMISSION_INTERVAL_MAX ? req->interval * 2 + STUN_RETRANSMISSION_INTERVAL_MIN : STUN_RETRANSMISSION_INTERVAL_MAX;
			req->elapsed += req->interval;
			if (req->elapsed > req->timeout)
				req->interval = req->timeout + req->interval - req->elapsed;
			// 11000 = STUN_TIMEOUT - (500 + 1500 + 3500 + 7500 + 15500)
			req->timer = stun_timer_start(req->interval, stun_request_ontimer, req);
			if (req->timer)
			{
				locker_unlock(&req->locker);
				return;
			}
		}

		assert(req->ref >= 2);
		req->state = STUN_REQUEST_DONE;
		stun_agent_remove(req->stun, req); // delete link
		req->handler(req->param, req, 408, "Request Timeout");
	}
	locker_unlock(&req->locker);
	stun_request_release(req);
}

int stun_request_send(struct stun_agent_t* stun, struct stun_request_t* req)
{
	int r;
	assert(stun && req);

	if (!atomic_cas32(&req->state, STUN_REQUEST_INIT, STUN_REQUEST_RUNNING))
	{
		assert(0);
		return -1; // running;
	}
	assert(1 == req->ref);
	stun_request_addref(req);

	r = stun_message_send(stun, &req->msg, req->addr.protocol, &req->addr.host, &req->addr.peer, &req->addr.relay);
	if (0 == r)
	{
		locker_lock(&req->locker);
		if (STUN_REQUEST_RUNNING == req->state)
		{
			assert(2 == req->ref);
			stun_request_addref(req);
			req->elapsed = 0;
			req->interval = STUN_RETRANSMISSION_INTERVAL_MIN;
			req->timer = stun_timer_start(STUN_RETRANSMISSION_INTERVAL_MIN, stun_request_ontimer, req);
		}
		locker_unlock(&req->locker);
	}
	else
	{
		stun_request_prepare(req);
	}

	stun_request_release(req);
	return r;
}
