#ifndef _turn_client_h_
#define _turn_client_h_

#include "sys/sock.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct turn_client_t turn_client_t;
typedef struct turn_request_t turn_request_t;

struct turn_client_handler_t
{
	int (*onbind)(void* param, int code, const char* msg);
	int (*onsharedsecret)(void* param, int code, const char* msg);
	int (*onindication)(void* param, int code, const char* msg);
};

turn_client_t* turn_client_create(struct turn_client_handler_t* handler);
int turn_client_destroy(turn_client_t* turn);

/// request bind/keepalive
/// @param[in] turn turn client
/// @return turn request object, NULL if failure
int turn_client_allocate(turn_client_t* turn, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, void* param);

int turn_client_refresh(turn_client_t* turn, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, int expired, void* param);
    
int turn_client_permission(turn_client_t* turn, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, const struct sockaddr_storage* peer, void* param);
    
int turn_client_channel(turn_client_t* turn, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, const struct sockaddr_storage* peer, uint16_t channel, void* param);
    
int turn_client_send(turn_client_t* turn, const struct sockaddr* addr, socklen_t addrlen, uint8_t* data, int bytes, const struct sockaddr* peer, void* param);
    
/// input turn server response data
int turn_client_input(turn_client_t* turn, const void* data, int size, const struct sockaddr* addr, socklen_t addrlen);

#ifdef __cplusplus
}
#endif
#endif /* !_turn_client_h_ */
