#ifndef _stun_internal_h_
#define _stun_internal_h_

#include "stun-agent.h"
#include "stun-attr.h"
#include "stun-proto.h"
#include "stun-message.h"
#include "sys/locker.h"
#include "list.h"
#include <stdint.h>
#include <assert.h>

/*
 0               1
 2  3  4 5 6 7 8 9 0 1 2 3 4 5
+--+--+-+-+-+-+-+-+-+-+-+-+-+-+
|M |M |M|M|M|C|M|M|M|C|M|M|M|M|
|11|10|9|8|7|1|6|5|4|0|3|2|1|0|
+--+--+-+-+-+-+-+-+-+-+-+-+-+-+
*/
#define STUN_MESSAGE_TYPE(mclass, method)	((((mclass) & 02) << 7) | (((mclass) & 01) << 4) | (((method) & 0xF80) << 2) | (((method) & 0x0070) << 1) | ((method) & 0x000F))
#define STUN_MESSAGE_CLASS(type)			((((type) >> 7) & 0x02) | (((type) >> 4) & 0x01))
#define STUN_MESSAGE_METHOD(type)			((((type) >> 2) & 0x0F80) | (((type) >> 1) & 0x0070) | ((type) & 0x000F))

struct stun_address_t
{
	int protocol; // STUN_PROTOCOL_UDP
	struct sockaddr_storage host; // local address
	struct sockaddr_storage peer; // remote address
	struct sockaddr_storage relay;
	struct sockaddr_storage reflexive;
};

struct stun_request_t
{
	struct list_head link; // 
	struct stun_message_t msg;

	int ref;
	int rfc; // version
	int timeout;
	void* timer;
	locker_t locker;
	stun_agent_t* stun;

	void* param;
	stun_request_handler handler;

	void* ondataparam;
	turn_agent_ondata ondata;
	
	struct stun_address_t addr;
	struct stun_credential_t auth;
};

struct stun_response_t
{
	struct stun_message_t msg;

	int rfc; // version
	void* ptr; // user-defined
	stun_agent_t* stun;

	struct stun_address_t addr;
	struct stun_credential_t auth;
};

struct stun_agent_t
{
	locker_t locker;
	struct list_head requests; // stun/turn requests
	struct list_head turnclients; // client allocations
	struct list_head turnservers; // server allocations
    struct list_head turnreserved; // reserved allocations

	int rfc; // rfc version
	int auth_term; // 0-short term, 1-long term

	// for RFC3489 CHANGE-REQUEST
	struct sockaddr_storage A1, A2;

	struct stun_agent_handler_t handler;
	void* param;
};

struct stun_request_t* stun_agent_find(struct stun_agent_t* stun, const struct stun_message_t* msg);
int stun_agent_insert(struct stun_agent_t* stun, struct stun_request_t* req);
int stun_agent_remove(struct stun_agent_t* stun, struct stun_request_t* req);

int stun_request_addref(struct stun_request_t* req);
int stun_request_release(struct stun_request_t* req);
int stun_request_send(struct stun_agent_t* stun, struct stun_request_t* req);
int stun_response_send(struct stun_agent_t* stun, struct stun_response_t* resp);
int stun_message_send(struct stun_agent_t* stun, struct stun_message_t* msg, int protocol, const struct sockaddr_storage* local, const struct sockaddr_storage* remote, const struct sockaddr_storage* relay);

struct stun_response_t* stun_response_create(struct stun_request_t* req);
int stun_response_destroy(struct stun_response_t** pp);

int stun_agent_auth(struct stun_agent_t* stun, struct stun_request_t* req, const void* data, int bytes);
int stun_server_onbind(struct stun_agent_t* stun, const struct stun_request_t* req, struct stun_response_t* resp);
int stun_server_onshared_secret(struct stun_agent_t* stun, const struct stun_request_t* req, struct stun_response_t* resp);
int stun_server_onbindindication(struct stun_agent_t* stun, const struct stun_request_t* req);

#endif /* !_stun_internal_h_ */
