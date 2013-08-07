#ifndef _tcpserver_h_
#define _tcpserver_h_

#include "sys/sock.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* tcpserver_t;

typedef struct
{
	void (*onconnected)(void* param, socket_t sock, const char* ip, int port);
	void (*onerror)(void* param, int errcode);
} tcpserver_handler_t;

tcpserver_t tcpserver_start(const char* ip, int port, tcpserver_handler_t* callback, void* param);
int tcpserver_stop(tcpserver_t server);

int tcpserver_setbacklog(tcpserver_t server, int num);
int tcpserver_setkeepalive(tcpserver_t server, int keepalive);

#ifdef __cplusplus
}
#endif

#endif /* !_tcpserver_h_ */
