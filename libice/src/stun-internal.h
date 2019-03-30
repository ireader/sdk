#ifndef _stun_internal_h_
#define _stun_internal_h_

#include "stun-agent.h"
#include "stun-attr.h"
#include "stun-proto.h"
#include "stun-message.h"
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
#define STUN_MESSAGE_TYPE(mclass, method)	((((mclass) & 02) << 7) | (((mclass) & 01) << 4) | (((method) & 0xF8) << 2) | (((method) & 0x0070) << 1) | ((method) & 0x000F))
#define STUN_MESSAGE_CLASS(type)			((((type) >> 7) & 0x02) | (((type) >> 4) & 0x01))
#define STUN_MESSAGE_METHOD(type)			((((type) >> 2) & 0x0F80) | (((type) >> 1) & 0x0070) | ((type) & 0x000F))

enum 
{
	STUN_PROTOCOL_UDP = 0,
	STUN_PROTOCOL_TCP,
	STUN_PROTOCOL_TLS,
};

struct stun_transaction_t
{
	struct list_head link; // 
	struct stun_message_t msg;

	int ref;
	int rfc; // version
	stun_agent_t* stun;

	void* param;
	stun_transaction_handler handler;
	
	int protocol; // STUN_PROTOCOL_UDP
	struct sockaddr_storage host;
	struct sockaddr_storage remote;
	struct sockaddr_storage relay;
	struct sockaddr_storage reflexive;

	struct stun_credetial_t auth;
};

struct stun_agent_t
{
	struct list_head root;

	// for RFC3489 CHANGE-REQUEST
	struct sockaddr_storage A1, A2;

	struct stun_agent_handler_t handler;
	void* param;
};

int stun_transaction_addref(struct stun_transaction_t* t);
int stun_transaction_release(struct stun_transaction_t* t);
int stun_transaction_destroy(struct stun_transaction_t** pp);
int stun_transaction_send(stun_agent_t* stun, stun_transaction_t* t);
struct stun_transaction_t* stun_response_create(struct stun_transaction_t* req);

int stun_message_send(stun_agent_t* stun, struct stun_message_t* msg, int protocol, const struct sockaddr_storage* local, const struct sockaddr_storage* remote);

int stun_agent_onbind(const struct stun_transaction_t* req, struct stun_transaction_t* resp);
int stun_agent_onshared_secret(const struct stun_transaction_t* req, struct stun_transaction_t* resp);

#endif /* !_stun_internal_h_ */
