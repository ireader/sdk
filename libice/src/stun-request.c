#include "stun-agent.h"
#include "stun-internal.h"
#include "sys/atomic.h"

stun_request_t* stun_request_create(int rfc, struct stun_request_handler_t* handler, void* param)
{
	stun_request_t* req;
	struct stun_message_t* msg;
	req = (stun_request_t*)calloc(1, sizeof(stun_request_t));
	if (!req) return NULL;

	req->ref = 1;
	req->rfc = rfc;
	req->param = param;
	memcpy(&req->handler, handler, sizeof(struct stun_request_handler_t));

	msg = &req->msg;
//	memset(msg, 0, sizeof(struct stun_message_t));
//	msg->header.msgtype = STUN_METHOD_BIND | STUN_METHOD_CLASS_REQUEST;
	msg->header.length = 0;
	msg->header.cookie = STUN_MAGIC_COOKIE;
	stun_transaction_id(msg->header.tid, sizeof(msg->header.tid));

	stun_message_add_string(msg, STUN_ATTR_SOFTWARE, STUN_SOFTWARE);
	return req;
}

int stun_request_destroy(stun_request_t* req)
{
	// CALL by user only

	// TODO: cancel request

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
		free(req);
	return 0;
}

int stun_request_setaddr(stun_request_t* req, int protocol, const struct sockaddr_storage* local, const struct sockaddr_storage* remote)
{
	if (!local || !remote || (STUN_PROTOCOL_UDP != protocol && STUN_PROTOCOL_TCP != protocol && STUN_PROTOCOL_TLS != protocol))
		return -1;

	req->protocol = protocol;
	memcpy(&req->host, local, sizeof(struct sockaddr_storage));
	memcpy(&req->remote, remote, sizeof(struct sockaddr_storage));
	return 0;
}

int stun_request_getaddr(stun_request_t* req, int* protocol, struct sockaddr_storage* local, struct sockaddr_storage* remote, struct sockaddr_storage* reflexive)
{
	if(protocol) *protocol = req->protocol;
	if (local) memcpy(local, &req->host, sizeof(struct sockaddr_storage));
	if (remote) memcpy(remote, &req->remote, sizeof(struct sockaddr_storage));
	if (reflexive) memcpy(reflexive, &req->reflexive, sizeof(struct sockaddr_storage));
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

int stun_request_getauth(stun_request_t* req, int *credential, char usr[256], char pwd[256], char realm[256], char nonce[256])
{
	if (credential) *credential = req->auth.credential;
	if (usr) snprintf(usr, 256, "%s", req->auth.usr);
	if (pwd) snprintf(usr, 256, "%s", req->auth.pwd);
	if (realm) snprintf(usr, 256, "%s", req->auth.realm);
	if (nonce) snprintf(usr, 256, "%s", req->auth.nonce);
	return 0;
}
