#ifndef _stun_internal_h_
#define _stun_internal_h_

#include "sys/sock.h"
#include "stun-agent.h"
#include "stun-attr.h"
#include "stun-proto.h"
#include "stun-message.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "list.h"
#include <stdint.h>
#include <assert.h>

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
	int state; // 0-init, 1-running, 2-done
	int timeout;
	int elapsed;
	int interval;
	int authtimes; // avoid too many auth failed
	void* timer;
	locker_t locker;
	stun_agent_t* stun;

	void* param;
	stun_request_handler handler;

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
	int auth_term; // STUN_CREDENTIAL_SHORT_TERM/STUN_CREDENTIAL_LONG_TERM

	// for RFC3489 CHANGE-REQUEST
	struct sockaddr_storage A1, A2;

	struct stun_agent_handler_t handler;
	void* param;
};

int stun_agent_input2(stun_agent_t* stun, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relayed, const void* data, int bytes);

int stun_agent_insert(struct stun_agent_t* stun, struct stun_request_t* req);
int stun_agent_remove(struct stun_agent_t* stun, struct stun_request_t* req);
int stun_agent_request_auth_check(stun_agent_t* stun, struct stun_request_t* req, const void* data, int bytes);
int stun_agent_response_auth_check(stun_agent_t* stun, const struct stun_message_t* resp, struct stun_request_t* req, const void* data, int bytes);

/// cancel a stun request, MUST make sure cancel before handler callback
/// @param[in] req create by stun_request_create
/// @return 0-ok, other-error
int stun_request_prepare(struct stun_request_t* req);
int stun_request_addref(struct stun_request_t* req);
int stun_request_release(struct stun_request_t* req);
int stun_request_send(struct stun_agent_t* stun, struct stun_request_t* req);
int stun_response_send(struct stun_agent_t* stun, struct stun_response_t* resp);
int stun_message_send(struct stun_agent_t* stun, struct stun_message_t* msg, int protocol, const struct sockaddr_storage* local, const struct sockaddr_storage* remote, const struct sockaddr_storage* relay);

struct stun_response_t* stun_response_create(stun_agent_t* stun, struct stun_request_t* req);
int stun_response_destroy(struct stun_response_t** pp);

int stun_server_onbind(struct stun_agent_t* stun, const struct stun_request_t* req, struct stun_response_t* resp);
int stun_server_onshared_secret(struct stun_agent_t* stun, const struct stun_request_t* req, struct stun_response_t* resp);
int stun_server_onbindindication(struct stun_agent_t* stun, const struct stun_request_t* req);

static inline const char* IP(const void* addr, char* ip)
{
	const struct sockaddr* sa;
	sa = (const struct sockaddr*)addr;
	return NULL == sa || 0 == sa->sa_family ? "" : inet_ntop(sa->sa_family, AF_INET == sa->sa_family ? (void*)&(((struct sockaddr_in*)sa)->sin_addr) : (void*)&(((struct sockaddr_in6*)sa)->sin6_addr), ip, 65);
}
static inline u_short PORT(const void* addr)
{
	const struct sockaddr* sa;
	sa = (const struct sockaddr*)addr;
	return NULL == sa || 0 == sa->sa_family ? 0 : ntohs(AF_INET == sa->sa_family ? ((const struct sockaddr_in*)sa)->sin_port : ((const struct sockaddr_in6*)sa)->sin6_port);
}

#endif /* !_stun_internal_h_ */
