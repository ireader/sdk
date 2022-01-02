#include "stun-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct stun_response_t* stun_response_create(stun_agent_t* stun, struct stun_request_t* req)
{
	stun_response_t* resp;
	resp = (stun_response_t*)calloc(1, sizeof(stun_response_t));
	if (!resp) return NULL;

	memcpy(&resp->msg.header, &req->msg.header, sizeof(struct stun_header_t));
	resp->msg.header.msgtype = STUN_MESSAGE_METHOD(req->msg.header.msgtype);
	resp->msg.header.length = 0;

	resp->stun = stun;
	//resp->addr.protocol = req->addr.protocol;
	//memcpy(&resp->addr.host, &req->addr.host, sizeof(struct sockaddr_storage));
	//memcpy(&resp->addr.peer, &req->addr.peer, sizeof(struct sockaddr_storage));
	//memcpy(&resp->addr.relay, &req->addr.relay, sizeof(struct sockaddr_storage));
	memcpy(&resp->addr, &req->addr, sizeof(struct stun_address_t));
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

int stun_response_send(struct stun_agent_t* stun, struct stun_response_t* resp)
{
	int r;
	r = stun_message_send(stun, &resp->msg, resp->addr.protocol, &resp->addr.host, &resp->addr.peer, &resp->addr.relay);
	stun_response_destroy(&resp);
	return r;
}

int stun_response_setauth(struct stun_response_t* resp, int credential, const char* usr, const char* pwd, const char* realm, const char* nonce)
{
	return stun_credential_setauth(&resp->auth, credential, usr, pwd, realm, nonce);
}
