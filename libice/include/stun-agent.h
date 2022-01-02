#ifndef _stun_agent_h_
#define _stun_agent_h_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stun_agent_t stun_agent_t;
typedef struct stun_message_t stun_message_t;
typedef struct stun_request_t stun_request_t;
typedef struct stun_response_t stun_response_t;

enum { STUN_RFC_3489, STUN_RFC_5389, };

enum { STUN_PROTOCOL_UDP, STUN_PROTOCOL_TCP, STUN_PROTOCOL_TLS, STUN_PROTOCOL_DTLS };

/// @param[in] req transaction request
/// @param[in] code http like code, 2xx-ok, 4xx/5xx-error
/// @param[in] phrase error code phrase
/// @return 0-ok, other-error
typedef int (*stun_request_handler)(void* param, const stun_request_t* req, int code, const char* phrase);

/// @param[in] rfc STUN rfc version: STUN_RFC_3489/STUN_RFC_5389
stun_request_t* stun_request_create(stun_agent_t* stun, int rfc, stun_request_handler handler, void* param);

/// @param[in] protocol 1-UDP, 2-TCP
/// @param[in] local local host address
/// @param[in] remote peer address
/// @param[in] relayed OPTIONAL turn relay address, NULL if don't use turn
int stun_request_setaddr(stun_request_t* req, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relayed);
int stun_request_getaddr(const stun_request_t* req, int* protocol, struct sockaddr_storage* local, struct sockaddr_storage* remote, struct sockaddr_storage* reflexive, struct sockaddr_storage* relayed);

/// @param[in] credential 0-Short-Term Credential Mechanism, 1-Long-Term Credential Mechanism
/// @param[in] realm Long-Term Credential only
/// @param[in] nonce Long-Term Credential only
/// @return 0-ok, other-error
int stun_request_setauth(stun_request_t* req, int credential, const char* usr, const char* pwd, const char* realm, const char* nonce);
/// stun_agent_shared_secret response username/password
/// @return 0-ok, other-error
int stun_request_getauth(const stun_request_t* req, char usr[512], char pwd[512], char realm[128], char nonce[128]);

/// @return stun message from request
const stun_message_t* stun_request_getmessage(const stun_request_t* req);

/// @param[in] timeout ms
void stun_request_settimeout(stun_request_t* req, int timeout);

struct stun_agent_handler_t
{
	/// UDP/TURN data callback
	/// @param[in] param user-defined parameter form turn_agent_allocate
	void(*ondata)(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes);

	/// @return 0-ok, other-error
	int (*send)(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes);

	/// get pwd
	/// @param[in] cred STUN_CREDENTIAL_SHORT_TERM/STUN_CREDENTIAL_LONG_TERM
	/// @param[in] realm/nonce, if usr is null, return realm/nonce
	/// @param[out] pwd password of the usr
	/// @return 0-ok, other-error(update realm/nonce)
	int (*auth)(void* param, int cred, const char* usr, const char* realm, const char* nonce, char pwd[512]);
	
	/// turn long-term credential get realm/nonce
	int (*getnonce)(void* param, char realm[128], char nonce[128]);

	// stun
	int (*onbind)(void* param, stun_response_t* resp, const stun_request_t* req);
	int (*onsharedsecret)(void* param, stun_response_t* resp, const stun_request_t* req);
	int (*onbindindication)(void* param, const stun_request_t* req);

	// TURN allocation
	/// @param[in] evenport 1-event port, other-nothing
	/// @param[in] nextport 1-reserve next higher port, other-nothing
	int (*onallocate)(void* param, stun_response_t* resp, const stun_request_t* req, int evenport, int nextport);
	int (*onrefresh)(void* param, stun_response_t* resp, const stun_request_t* req, int lifetime);
	int (*onpermission)(void* param, stun_response_t* resp, const stun_request_t* req, const struct sockaddr* peer);
	int (*onchannel)(void* param, stun_response_t* resp, const stun_request_t* req, const struct sockaddr* peer, uint16_t channel);
};

stun_agent_t* stun_agent_create(int rfc, struct stun_agent_handler_t* handler, void* param);
int stun_agent_destroy(stun_agent_t** stun);

/// @param[in] protocol transport protocol(STUN_PROTOCOL_UDP/STUN_PROTOCOL_TCP/STUN_PROTOCOL_XXX)
/// @param[in] local local host address
/// @param[in] remote peer host address
/// @return 0-ok, other-error
int stun_agent_input(stun_agent_t* stun, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes);

/// STUN Bind
/// @return 0-ok, other-error
int stun_agent_bind(stun_request_t* req);
/// STUN SharedSecret(deprecated)
/// @return 0-ok, other-error
int stun_agent_shared_secret(stun_request_t* req);

/// TURN
/// Allocate relay address, identify by local address with remote server address
int turn_agent_allocate(stun_request_t* req, int peertransport);
/// @param[in] expired 0-free allocate, >0-update expired time(current + expired)
int turn_agent_refresh(stun_request_t* req, int expired);
int turn_agent_create_permission(stun_request_t* req, const struct sockaddr* peer);
/// @param[in] channel valid range: [0x4000, 0x7FFF]
int turn_agent_channel_bind(stun_request_t* req, const struct sockaddr* peer, uint16_t channel);
/// Send data from client to peer (relay by turn server)
/// @param[in] relay turn server relayed address(not the stun/turn bind address)
/// @param[in] peer remote host address
int turn_agent_send(stun_agent_t* stun, const struct sockaddr* relay, const struct sockaddr* peer, const void* data, int bytes);

// RESPONSE

int stun_response_setauth(struct stun_response_t* resp, int credential, const char* usr, const char* pwd, const char* realm, const char* nonce);

/// ignore request
int stun_agent_discard(struct stun_response_t* resp);

int stun_agent_bind_response(struct stun_response_t* resp, int code, const char* pharse);
int stun_agent_shared_secret_response(struct stun_response_t* resp, int code, const char* pharse, const char* usr, const char* pwd);
int turn_agent_allocate_response(struct stun_response_t* resp, const struct sockaddr* relay, int code, const char* pharse);
int turn_agent_refresh_response(struct stun_response_t* resp, int code, const char* pharse);
int turn_agent_create_permission_response(struct stun_response_t* resp, int code, const char* pharse);
int turn_agent_channel_bind_response(struct stun_response_t* resp, int code, const char* pharse);

void* stun_timer_start(int ms, void(*ontimer)(void* param), void* param);
/// @return  0-ok, other-timer can't be stop(timer have triggered or will be triggered)
int stun_timer_stop(void* timer);

#ifdef __cplusplus
}
#endif

#endif /* !_stun_agent_h_ */
