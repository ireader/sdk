#include "aio-socket.h"
#include "thread-pool.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

static int s_epoll = -1;

enum epoll_action {
	ACTION_NONE			= 0,
	ACTION_ACCEPT		= 0x0001,
	ACTION_CONNECT		= 0x0002,
	ACTION_RECV			= 0x0010,
	ACTION_SEND			= 0x0020,
	ACTION_RECV_V		= 0x0040,
	ACTION_SEND_V		= 0x0080,
	ACTION_RECVFROM		= 0x0100,
	ACTION_SENDTO		= 0x0200,
	ACTION_RECVFROM_V	= 0x0400,
	ACTION_SENDTO_V		= 0x0800,
};

struct epoll_context_accept
{
	aio_onaccept proc;
	void *param;
};

struct epoll_context_connect
{
	aio_onconnect proc;
	void *param;
};

struct epoll_context_recv
{
	aio_onrecv proc;
	void *param;
};

struct epoll_context_send
{
	aio_onsend proc;
	void *param;
};

struct epoll_context
{
	struct epoll_event ev;
	socket_t socket;
	int own;
	int action_read;
	int action_write;

	epoll_context_accept accept;
	epoll_context_connect connect;
	epoll_context_recv recv;
	epoll_context_send send;
};

int aio_socket_init()
{
	s_epoll = epoll_create(INTMAX);
	return -1 == s_epoll ? errno : 0;
}

int aio_socket_clean()
{
	if(-1 != s_epoll)
		close(s_epoll);
	return 0;
}

aio_socket_t aio_socket_create(socket_t socket, int own)
{
	int flags;
	epoll_context* ctx;
	ctx = (epoll_context*)malloc(sizeof(epoll_context));
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(epoll_context));
	ctx->own = own;
	ctx->socket = socket;
	ctx->ev.events = 0;
	ctx->ev.data.ptr = ctx;

	if(0 != epoll_ctl(s_epoll, EPOLL_CTL_ADD, socket, &ctx->ev))
	{
		free(ctx);
		return NULL;
	}

	// set non-blocking socket
	flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	return ctx;
}

int aio_socket_close(aio_socket_t socket)
{
	epoll_context* ctx = (epoll_context*)socket;
	assert(ctx->ev.data.ptr == ctx);

	if(0 != epoll_ctl(s_epoll, EPOLL_CTL_DEL, ctx->socket, &ctx->ev))
	{
		assert(false);
		return errno;
	}

	if(ctx->own)
		closesocket(ctx->socket);
	free(ctx);
	return 0;
}

int aio_socket_cancel(aio_socket_t socket)
{
}

int aio_socket_accept(aio_socket_t socket, const char* ip, int port, aio_onaccept proc, void* param)
{
	int r;
	char ip[16];
	socket_t client;
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	epoll_context* ctx = (epoll_context*)socket;

	client = accept(ctx->socket, &addr, &addrlen);
	if(client > 0)
	{
		sprintf(ip, "%d.%d.%d.%d", addr.sin_addr.s_net, addr.sin_addr.s_host, addr.sin_addr.s_lh, addr.sin_addr.s_impno);
		proc(param, 0, client, ip, ntohs(addr.sin_port));
		return 0;
	}

	assert(-1 == client);
	if(EAGAIN == errno)
	{
		ctx->accept.proc = proc;
		ctx->accept.param = param;
		ctx->action |= ACTION_ACCEPT;
		ctx->ev.events |= EPOLLIN; // man 2 accept to see more
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action |= (ctx->action & (~ACTION_ACCEPT));
		ctx->ev.events = (ctx->ev.events & (~EPOLLIN));
	}

	return errno;
}

int aio_socket_connect(aio_socket_t socket, const char* ip, int port, aio_onconnect proc, void* param)
{
	int r, errcode;
	char ip[16];
	socket_t client;
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	epoll_context* ctx = (epoll_context*)socket;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	r = connect(ctx->socket, &addr, &addrlen);
	if(0 == r)
	{
		sprintf(ip, "%d.%d.%d.%d", addr.sin_addr.s_net, addr.sin_addr.s_host, addr.sin_addr.s_lh, addr.sin_addr.s_impno);
		proc(param, 0, client, ip, ntohs(addr.sin_port));
		return 0;
	}

	errcode = errno;
	if(EINPROGRESS == errno)
	{
		ctx->connect.proc = proc;
		ctx->connect.param = param;
		ctx->action |= ACTION_CONNECT;
		ctx->ev.events |= EPOLLOUT; // man 2 connect to see more(ERRORS: EINPROGRESS)
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action |= (ctx->action & (~ACTION_CONNECT));
		ctx->ev.events = (ctx->ev.events & (~EPOLLOUT));

		addrlen = sizeof(errcode);
		getsockopt(ctx->socket, SOL_SOCKET, SO_ERROR, (void*)&errcode, (socklen_t*)&addrlen);
	}

	return errcode;
}

int aio_socket_send(aio_socket_t socket, const void* buffer, int bytes, aio_onsend proc, void* param)
{
	int r;
	epoll_context* ctx = (epoll_context*)socket;

	r = send(ctx->socket, buffer, bytes, 0);
	if(-1 != r)
	{
		proc(param, 0, r);
		return 0;
	}

	errcode = errno;
	if(EAGAIN == errno)
	{
		ctx->send.proc = proc;
		ctx->send.param = param;
		ctx->action |= ACTION_SEND;
		ctx->ev.events |= EPOLLOUT;
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action = (ctx->action & (~ACTION_SEND));
		ctx->ev.events = (ctx->ev.events & (~EPOLLOUT));
	}

	return errcode;
}

int aio_socket_recv(aio_socket_t socket, void* buffer, int bytes, aio_onrecv proc, void* param)
{
}

int aio_socket_send_v(aio_socket_t socket, const socket_bufvec_t* vec, size_t n, aio_onsend proc, void* param)
{
}

int aio_socket_recv_v(aio_socket_t socket, socket_bufvec_t* vec, size_t n, aio_onrecv proc, void* param)
{
}
