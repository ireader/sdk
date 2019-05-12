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

	msg = &req->msg;
//	memset(msg, 0, sizeof(struct stun_message_t));
//	msg->header.msgtype = STUN_METHOD_BIND | STUN_METHOD_CLASS_REQUEST;
	msg->header.length = 0;
	msg->header.cookie = STUN_MAGIC_COOKIE;
	stun_transaction_id(msg->header.tid, sizeof(msg->header.tid));

	stun_message_add_string(msg, STUN_ATTR_SOFTWARE, STUN_SOFTWARE);
	return req;
}

int stun_request_destroy(struct stun_request_t** pp)
{
	struct stun_request_t* req;
	if (!pp || !*pp)
		return -1;
	req = *pp;

	// delete link
	stun_agent_remove(req->stun, req);

	free(req);
	*pp = NULL;
	return 0;
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
		free(req);
	return 0;
}

struct stun_response_t* stun_response_create(struct stun_request_t* req)
{
	stun_response_t* resp;
	resp = (stun_response_t*)calloc(1, sizeof(stun_response_t));
	if (!resp) return NULL;

	memcpy(&resp->msg.header, &req->msg.header, sizeof(struct stun_header_t));
	resp->msg.header.msgtype = STUN_MESSAGE_METHOD(req->msg.header.msgtype);
	resp->msg.header.length = 0;
	
	resp->stun = req->stun;
	resp->addr.protocol = req->addr.protocol;
	memcpy(&resp->addr.host, &req->addr.host, sizeof(struct sockaddr_storage));
	memcpy(&resp->addr.peer, &req->addr.peer, sizeof(struct sockaddr_storage));
	memcpy(&resp->auth, &req->auth, sizeof(struct stun_credential_t));
	return resp;
}

int stun_response_destroy(struct stun_response_t** pp)
{
	struct stun_response_t* resp;
	if (!pp || !*pp)
		return -1;

	resp = *pp;
	free(resp);
	*pp = NULL;
	return 0;
}

int stun_agent_discard(struct stun_response_t* resp)
{
	return stun_response_destroy(&resp);
}

int stun_request_setaddr(stun_request_t* req, int protocol, const struct sockaddr* local, const struct sockaddr* remote)
{
	if (!remote || (STUN_PROTOCOL_UDP != protocol && STUN_PROTOCOL_TCP != protocol && STUN_PROTOCOL_TLS != protocol))
	{
		assert(0);
		return -1;
	}

	req->addr.protocol = protocol;
	memcpy(&req->addr.host, local, local ? socket_addr_len(local) : 0);
	memcpy(&req->addr.peer, remote, remote ? socket_addr_len(remote) : 0);
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
