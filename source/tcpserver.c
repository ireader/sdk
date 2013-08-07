#include <stdio.h>
#include <assert.h>
#include "cstringext.h"
#include "tcpserver.h"
#include "sys/process.h"

typedef struct _tcpserver_info
{
	int run;
	thread_t thread;

	int backlog;
	int keepalive;

	void* cbparam;
	tcpserver_handler_t handler;

	char ip[128];
	unsigned short port;
} tcpserver_info;

static int worker(IN void* param)
{
	int ret;
	socket_t server;
	socket_t client;
	socklen_t len;
	struct sockaddr_in addr;
	tcpserver_info* ctx;

	ctx = (tcpserver_info*)param;

	// new tcp socket
	server = socket_tcp();
	if(socket_error == server)
	{
		ctx->handler.onerror(ctx->cbparam, socket_geterror());
		return 0;
	}

	// reuse addr
	socket_setreuseaddr(server, 1);

	// bind
	if(0 == ctx->ip[0])
	{
		ret = socket_bind_any(server, ctx->port);
	}
	else
	{
		ret = socket_addr_ipv4(&addr, ctx->ip, ctx->port);
		if(0 == ret)
			ret = socket_bind(server, (struct sockaddr*)&addr, sizeof(addr));
	}
	if(0 != ret)
	{
		ctx->handler.onerror(ctx->cbparam, socket_geterror());
		socket_close(server);
		return 0;
	}

	// listen
	ret = socket_listen(server, ctx->backlog);
	if(0 != ret)
	{
		ctx->handler.onerror(ctx->cbparam, socket_geterror());
		socket_close(server);
		return 0;
	}

	// wait for client
	printf("server listen at %s.%d\n", ctx->ip[0]?ctx->ip:"localhost", ctx->port);

	while(ctx->run)
	{
		ret = socket_select_read(server, 60*1000);
		if(socket_error == ret)
		{
			ctx->handler.onerror(ctx->cbparam, socket_geterror());
			break;
		}
		else if(0 == ret)
		{
			// timeout
			continue;
		}

		len = sizeof(addr);
		client = socket_accept(server, (struct sockaddr*)&addr, &len);
		if(socket_invalid == client)
		{
			ctx->handler.onerror(ctx->cbparam, socket_geterror());
			break;
		}

		ctx->handler.onconnected(ctx->cbparam, client, inet_ntoa(addr.sin_addr), (int)ntohs(addr.sin_port));
	}

	socket_close(server);
	return 0;
}

tcpserver_t tcpserver_start(const char* ip, int port, tcpserver_handler_t* callback, void* param)
{
	tcpserver_info* ctx;

	socket_init();

	ctx = (tcpserver_info*)malloc(sizeof(tcpserver_info));
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(tcpserver_info));
	ctx->run = 1;
	ctx->backlog = 50;
	ctx->keepalive = 1;
	ctx->cbparam = param;
	memcpy(&ctx->handler, callback, sizeof(tcpserver_handler_t));
	ctx->port = (unsigned short)port;
	if(ip) strncpy(ctx->ip, ip, sizeof(ctx->ip)-1);

	if(0 != thread_create(&ctx->thread, worker, ctx))
	{
		free(ctx);
		return NULL;
	}
	return ctx;
}

int tcpserver_stop(tcpserver_t server)
{
	tcpserver_info* ctx = (tcpserver_info*)server;

	ctx->run = 0;
	thread_destroy(ctx->thread);
	socket_cleanup();
	free(ctx);
	return 0;
}

int tcpserver_setbacklog(tcpserver_t server, int num)
{
	tcpserver_info* ctx = (tcpserver_info*)server;
	if(num < 1)
		return -1;
	
	ctx->backlog = num;
	return 0;
}

int tcpserver_setkeepalive(tcpserver_t server, int keepalive)
{
	tcpserver_info* ctx = (tcpserver_info*)server;
	ctx->keepalive = keepalive;
	return 0;
}
