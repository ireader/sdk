#ifndef _stun_client_h_
#define _stun_client_h_

#include "sys/sock.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stun_client_t stun_client_t;
typedef struct stun_request_t stun_request_t;

enum { STUN_RFC_3489, STUN_RFC_5389 };

struct stun_client_handler_t
{
	int (*onbind)(void* param, int code, const char* msg);
	int (*onsharedsecret)(void* param, int code, const char* msg);
	int (*onindication)(void* param, int code, const char* msg);
};

/// @param[in] rfc STUN rfc version: STUN_RFC_3489/STUN_RFC_5389
stun_client_t* stun_client_create(int rfc, struct stun_client_handler_t* handler);
int stun_client_destroy(stun_client_t* stun);

/// request bind/keepalive
/// @param[in] stun STUN client
/// @return stun request object, NULL if failure
int stun_client_bind(stun_client_t* stun, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, void* param);

/// request shared secret(RFC3489 only)
/// @param[in] stun STUN client
/// @return stun request object, NULL if failure
int stun_client_shared_secret(stun_client_t* stun, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, void* param);

/// client indication
int stun_client_indication(stun_client_t* stun, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, void* param);

/// input stun server response data
int stun_client_input(stun_client_t* stun, const void* data, int size, const struct sockaddr* addr, socklen_t addrlen);

#ifdef __cplusplus
}
#endif
#endif /* !_stun_client_h_ */
