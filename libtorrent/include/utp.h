#ifndef _utp_h_
#define _utp_h_

#include <stdint.h>
#include "sys/sock.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct utp_t;
struct utp_socket_t;

struct utp_hander_t
{
	void (*onconnect)(void* param, int code, struct utp_socket_t* socket);
	void (*ondisconnect)(void* param, int code, struct utp_socket_t* socket);

	int (*onrecv)(void* param, struct utp_socket_t* socket, int code, const uint8_t* data, unsigned int bytes);
	int (*onsend)(void* param, struct utp_socket_t* socket, int code, const uint8_t* data, unsigned int bytes);
};

struct utp_t* utp_create(const uint16_t port, struct utp_hander_t* handler, void* param);
void utp_destroy(struct utp_t* utp);

int utp_input(struct utp_t* utp, const uint8_t* data, unsigned int bytes, const struct sockaddr_storage* addr);

// create utp connection, will call onconnect
int utp_connect(struct utp_t* utp, const struct sockaddr_storage* addr);

// disconnect a utp connection, will call ondisconnect
int utp_disconnect(struct utp_socket_t* socket);

// send data by utp
int utp_send(struct utp_socket_t* socket, const uint8_t* data, unsigned int bytes);

#if defined(__cplusplus)
}
#endif
#endif /* !_utp_h_ */
