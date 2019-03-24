#include "stun-internal.h"

struct stun_response_t* stun_response_create(struct stun_request_t* req)
{
	stun_response_t* resp;
	struct stun_message_t* msg;
	resp = (stun_response_t*)calloc(1, sizeof(stun_response_t));
	if (!resp) return NULL;

	resp->msg.header.msgtype = STUN_MESSAGE_METHOD(req->msg.header.msgtype);
	memcpy(resp->msg.header.tid, req->msg.header.tid, sizeof(resp->msg.header.tid));

	resp->protocol = req->protocol;
	memcpy(&resp->local, &req->remote, sizeof(struct sockaddr_storage));
	memcpy(&resp->remote, &req->reflexive, sizeof(struct sockaddr_storage));
	memcpy(&resp->auth, &req->auth, sizeof(struct stun_credetial_t));
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
