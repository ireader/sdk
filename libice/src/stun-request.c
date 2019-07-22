#include "stun-agent.h"
#include "stun-internal.h"
#include "sys/atomic.h"
#include <stdlib.h>
#include <assert.h>

stun_request_t* stun_request_create(stun_agent_t* stun, int rfc, stun_request_handler handler, void* param)
{
	stun_request_t* req;
	struct stun_message_t* msg;
	req = (stun_request_t*)calloc(1, sizeof(stun_request_t));
	if (!req) return NULL;

	req->ref = 1;
	req->rfc = rfc;
	req->stun = stun;
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
	return req;
}

int stun_request_destroy(struct stun_request_t* req)
{
	// wait for handler callback
	locker_lock(&req->locker);
	if (req->timer)
		stun_timer_stop(req->timer);
	req->handler = NULL;
	locker_unlock(&req->locker);

	return stun_request_release(req);
}

int stun_request_addref(struct stun_request_t* req)
{
	assert(req->ref > 0);
	if (atomic_increment32(&req->ref) <= 1)
	{
		//stun_request_release(req);
		return -1;
	}
	return 0;
}

int stun_request_release(struct stun_request_t* req)
{
	if (0 == atomic_decrement32(&req->ref))
	{
		// delete link
		//stun_agent_remove(req->stun, req);
		assert(NULL == req->link.prev && NULL == req->link.next);

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
	int r;
	if (!usr || !*usr || !pwd || !*pwd || (STUN_CREDENTIAL_SHORT_TERM != credential && STUN_CREDENTIAL_LONG_TERM != credential))
		return -1;
	
	req->auth.credential = credential;
	r = snprintf(req->auth.usr, sizeof(req->auth.usr), "%s", usr);
	if (r < 0 || r >= sizeof(req->auth.usr)) return -1;
	r = snprintf(req->auth.pwd, sizeof(req->auth.pwd), "%s", pwd);
	if (r < 0 || r >= sizeof(req->auth.pwd)) return -1;

	if (STUN_CREDENTIAL_LONG_TERM == credential)
	{
		if (!realm || !*realm || !nonce || !*nonce)
			return -1;

		r = snprintf(req->auth.realm, sizeof(req->auth.realm), "%s", realm);
		if (r < 0 || r >= sizeof(req->auth.realm)) return -1;
		r = snprintf(req->auth.nonce, sizeof(req->auth.nonce), "%s", nonce);
		if (r < 0 || r >= sizeof(req->auth.nonce)) return -1;
	}

	return 0;
}

int stun_request_getauth(const stun_request_t* req, char usr[512], char pwd[512], char realm[128], char nonce[128])
{
	if (usr) snprintf(usr, 512, "%s", req->auth.usr);
	if (pwd) snprintf(pwd, 512, "%s", req->auth.pwd);
    if (realm) snprintf(realm, 128, "%s", req->auth.realm);
    if (nonce) snprintf(nonce, 128, "%s", req->auth.nonce);
	return 0;
}

// rfc5389 7.2.1. Sending over UDP (p13)
static void stun_request_ontimer(void* param)
{
	stun_request_t* req;
	req = (stun_request_t*)param;
	locker_lock(&req->locker);
	if (req->handler /*running*/)
	{
		req->timeout = req->timeout * 2 + STUN_RETRANSMISSION_INTERVAL_MIN;
		if (req->timeout <= STUN_RETRANSMISSION_INTERVAL_MAX)
		{
			// 11000 = STUN_TIMEOUT - (500 + 1500 + 3500 + 7500 + 15500)
			req->timer = stun_timer_start(req->timeout <= STUN_RETRANSMISSION_INTERVAL_MAX ? req->timeout : 11000, stun_request_ontimer, req);
			if (req->timer)
			{
				locker_unlock(&req->locker);
				return;
			}
		}

		req->handler(req->param, req, 408, "Request Timeout");
	}
	locker_unlock(&req->locker);

	stun_agent_remove(req->stun, req);
}

int stun_request_send(struct stun_agent_t* stun, struct stun_request_t* req)
{
	int r;
	assert(stun && req);

	if (0 != stun_agent_insert(stun, req))
		return -1;

	locker_lock(&req->locker);
	if (NULL != req->timer)
	{
		locker_unlock(&req->locker);
		return -1; // exist
	}

	req->timeout = STUN_RETRANSMISSION_INTERVAL_MIN;
	req->timer = stun_timer_start(STUN_RETRANSMISSION_INTERVAL_MIN, stun_request_ontimer, req);
	locker_unlock(&req->locker);

	r = stun_message_send(stun, &req->msg, req->addr.protocol, &req->addr.host, &req->addr.peer, &req->addr.relay);
	if (0 != r)
	{
		locker_lock(&req->locker);
		if(req->timer)
			stun_timer_stop(req->timer);
		req->timer = NULL;
		locker_unlock(&req->locker);

		stun_agent_remove(stun, req);
	}

	return r;
}
