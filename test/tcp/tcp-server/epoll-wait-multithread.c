#include "sys/sock.h"
#include "sys/system.h"
#include "sys/process.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

// gcc epoll-wait-multithread.c -o epoll-wait-multithread -I../../../include -lrt -lpthread -ldl

#define MAX_EVENT 64
#define MAX_THREAD 4

static int s_epoll = -1;

struct epoll_context
{
	socket_t socket;
	struct epoll_event ev;
	void (*action)(struct epoll_context* ctx);
};

static void OnRead(struct epoll_context* ctx)
{
	char msg[64] = {0};
	char reply[64] = {0};

	system_sleep(5000);

	recv(ctx->socket, msg, sizeof(msg)-1, 0);
	printf("[%u-%u] recv msg: %s\n", thread_self(), ctx->socket, msg);

	sprintf(reply, "[%u-%u] reply.\n", thread_self(), ctx->socket);
	send(ctx->socket, reply, strlen(reply), 0);
}

static void OnAccept(struct epoll_context* ctx)
{
	socket_t client;
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	struct epoll_context * aio;

	aio = (struct epoll_context*)malloc(sizeof(struct epoll_context));
	memset(aio, 0, sizeof(struct epoll_context));
	aio->socket = accept(ctx->socket, (struct sockaddr*)&addr, &addrlen);
	aio->action = OnRead;
	aio->ev.events = EPOLLIN|EPOLLET;
	aio->ev.data.ptr = aio;

//	socket_setnonblock(aio->socket, 1);

	printf("[%u] accept %s:%d\n", thread_self(), inet_ntoa(addr.sin_addr), addr.sin_port);
	epoll_ctl(s_epoll, EPOLL_CTL_ADD, aio->socket, &aio->ev);
}

static int OnThread(void* param)
{
	int i, r;
	struct epoll_context* ctx;
	struct epoll_event events[MAX_EVENT];

	while(1)
	{
		r = epoll_wait(s_epoll, events, MAX_EVENT, -1);
		printf("[%u] epoll wait return: %d\n", thread_self(), r);
		for(i = 0; i < r; i++)
		{
			ctx = (struct epoll_context*)events[i].data.ptr;
			ctx->action(ctx);
		}
	}

	return 0;
}

static socket_t CreateServerSocket(int port)
{
	int r;
	socket_t server;
	struct sockaddr_in addr;

	server = socket_tcp();
	if(socket_error == server)
		return socket_invalid;

	// reuse addr
	r = socket_setreuseaddr(server, 1);

	// bind
	r = socket_bind_any(server, port);

	// listen
	r = socket_listen(server, 64);

	return server;
}

int main(int argc, char* argv[])
{
	int i;
	socket_t server;
	struct epoll_context ctx;
	
	server = CreateServerSocket(5000);

	s_epoll = epoll_create(INT_MAX);

	// accept
	memset(&ctx, 0, sizeof(ctx));
	ctx.socket = server;
	ctx.action = OnAccept;
	ctx.ev.events = EPOLLIN|EPOLLET;
	ctx.ev.data.ptr = &ctx;
	epoll_ctl(s_epoll, EPOLL_CTL_ADD, server, &ctx.ev);

	thread_t threads[MAX_THREAD];
	for(i=0; i<MAX_THREAD; i++)
		thread_create(&threads[i], OnThread, NULL);

	while('q' != getchar());

	for(i=0; i<MAX_THREAD; i++)
		thread_destroy(threads[i]);

	epoll_ctl(s_epoll, EPOLL_CTL_DEL, server, &ctx.ev);
	socket_close(server);
	close(s_epoll);
	return server;
}
