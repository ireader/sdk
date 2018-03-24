#ifndef _utp_h_
#define _utp_h_

#include <stdint.h>
#include "sys/sock.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct utp_t;
struct utp_socket_t;

typedef int(*utp_onconnected)(void* param, struct utp_socket_t* socket);

struct utp_hander_t
{
	void (*onconnect)(void* param);
	void (*ondestroy)(void* param);

	int (*onrecv)(void* param, int code, const uint8_t* data, unsigned int bytes);
	int (*onsend)(void* param, int code, const uint8_t* data, unsigned int bytes);
};

struct utp_t* utp_create(const uint16_t port, utp_onconnected onconnected, void* param);
void utp_destroy(struct utp_t* utp);

int utp_input(struct utp_t* utp, const uint8_t* data, unsigned int bytes, const struct sockaddr_storage* addr);

struct utp_socket_t* utp_socket_create(struct utp_t* utp);
void utp_socket_destroy(struct utp_socket_t* socket);

void utp_socket_sethandler(struct utp_socket_t* socket, struct utp_hander_t* handler, void* param);

/// uTP bind(server mode)
int utp_socket_bind(struct utp_socket_t* socket, const struct sockaddr_storage* addr);

/// uTP syn
int utp_socket_connect(struct utp_socket_t* socket, const struct sockaddr_storage* addr);

/// uTP data
int utp_socket_send(struct utp_socket_t* socket, const uint8_t* data, unsigned int bytes);

#if defined(__cplusplus)
}
#endif
#endif /* !_utp_h_ */
