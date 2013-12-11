#include "aio-socket.h"
#include "thread-pool.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

static int s_epoll = -1;

struct epoll_context_accept
{
	aio_onaccept proc;
	void *param;
};

struct epoll_context_connect
{
	aio_onconnect proc;
	void *param;
	struct sockaddr_in addr;
};

struct epoll_context_recv
{
	aio_onrecv proc;
	void *param;
	void *buffer;
	int bytes;
};

struct epoll_context_send
{
	aio_onsend proc;
	void *param;
	void *buffer;
	int bytes;
};

struct epoll_context_recv_v
{
	aio_onrecv proc;
	void *param;
	socket_bufvec_t *vec;
	int n;
};

struct epoll_context_send_v
{
	aio_onsend proc;
	void *param;
	socket_bufvec_t *vec;
	int n;
};

struct epoll_context
{
	struct epoll_event ev;
	socket_t socket;
	int own;
	int ref;
	int closed;

	void (*action_in)(struct epoll_context *ctx);
	void (*action_out)(struct epoll_context *ctx);

	union
	{
		epoll_context_accept accept;
		epoll_context_recv recv;
		epoll_context_recv_v recv_v;
	} in;

	union
	{
		epoll_context_connect connect;
		epoll_context_send send;
		epoll_context_send_v send_v;
	} out;
};

#define MAX_EVENT 64

static int epoll_process()
{
	int i, r;
	epoll_context* ctx;
	struct epoll_event events[1];
	void (*action_in)(struct epoll_context *ctx);
	void (*action_out)(struct epoll_context *ctx);

	r = epoll_wait(s_epoll, events, 1, 2000);
	for(i = 0; i < r; i++)
	{
		ctx = (epoll_context*)events[i].data.ptr;

		if(EPOLLIN & events[i].events)
		{
			action_in = ctx->action_in;
			ctx->action_in = 0;
			ctx->ev.events &= ~EPOLLIN; // clear event
		}
		
		if(EPOLLOUT & events[i].events)
		{
			action_out = ctx->action_out;
			ctx->action_out = 0;
			ctx->ev.events &= ~EPOLLOUT; // clear event
		}

		// modify events
		epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev);

		// do read action
		if(action_in)
			action_in(ctx);

		// do write action
		if(action_out)
			action_out(ctx);
	}
}

int aio_socket_init()
{
	s_epoll = epoll_create(64);
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
	ctx->ref = 1;
	ctx->own = own;
	ctx->socket = socket;
	ctx->ev.events = EPOLLET; // Edge Triggered, for multi-thread epoll_wait(see more at epoll-wait-multithread.c)
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

static int epoll_accept(epoll_context* ctx)
{
	char ip[16];
	socket_t client;
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);

	client = accept(ctx->socket, &addr, &addrlen);
	if(client > 0)
	{
		strcpy(ip, inet_ntoa(addr.sin_addr));
		ctx->in.accept.proc(ctx->in.accept.param, 0, client, ip, ntohs(addr.sin_port));
		return 0;
	}
	else
	{
		assert(-1 == client);
		ctx->in.accept.proc(ctx->in.accept.param, errno, 0, "", 0);
		return errno;
	}
}

int aio_socket_accept(aio_socket_t socket, const char* ip, int port, aio_onaccept proc, void* param)
{
	int r;
	epoll_context* ctx = (epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLIN));
	assert(0 == ctx->action_in);
	if(0 != ctx->action_in)
		return EIO;

	ctx->in.accept.proc = proc;
	ctx->in.accept.param = param;
	if(0 == epoll_accept(ctx))
		return 0;

	if(EAGAIN == errno)
	{
		ctx->action_in = epoll_accept;
		ctx->ev.events |= EPOLLIN; // man 2 accept to see more
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action_in = 0;
		ctx->ev.events &= ~EPOLLIN;
	}

	return errno;
}

static int epoll_connect(epoll_context* ctx)
{
	int r;
	int addrlen = sizeof(ctx->out.connect.addr);
	
	r = connect(ctx->socket, &ctx->out.connect.addr, &addrlen);
	if(0 == r)
	{
		addrlen = sizeof(r);
		getsockopt(ctx->socket, SOL_SOCKET, SO_ERROR, (void*)&r, (socklen_t*)&addrlen);

		ctx->out.connect.proc(ctx->out.connect.param, r);
	}

	return r;
}

int aio_socket_connect(aio_socket_t socket, const char* ip, int port, aio_onconnect proc, void* param)
{
	epoll_context* ctx = (epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLOUT));
	assert(0 == ctx->action_out);	
	if(0 != ctx->action_out)
		return EIO;

	ctx->out.connect.addr.sin_family = AF_INET;
	ctx->out.connect.addr.sin_port = htons(port);
	ctx->out.connect.addr.sin_addr.s_addr = inet_addr(ip);
	ctx->out.connect.proc = proc;
	ctx->out.connect.param = param;

	if(0 == epoll_connect(ctx))
		return 0;

	if(EINPROGRESS == errno)
	{
		ctx->action_out = epoll_connect;
		ctx->ev.events |= EPOLLOUT; // man 2 connect to see more(ERRORS: EINPROGRESS)
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action_out = 0;
		ctx->ev.events &= ~EPOLLOUT;
	}

	return errno;
}

static int epoll_send(epoll_context* ctx)
{
	int r;
	r = send(ctx->socket, ctx->out.send.buffer, ctx->out.send.bytes, 0);
	if(r >= 0)
		ctx->out.send.proc(ctx->out.send.param, 0, r);

	return 0;
}

int aio_socket_send(aio_socket_t socket, const void* buffer, int bytes, aio_onsend proc, void* param)
{
	epoll_context* ctx = (epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLOUT));
	assert(0 == ctx->action_out);	
	if(0 != ctx->action_out)
		return EIO;

	ctx->out.send.proc = proc;
	ctx->out.send.param = param;
	ctx->out.send.buffer = buffer;
	ctx->out.send.bytes = bytes;

	if(0 <= epoll_send(ctx))
		return 0;

	if(EAGAIN == errno)
	{
		ctx->action_out = epoll_send;
		ctx->ev.events |= EPOLLOUT;
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action_out = 0;
		ctx->ev.events &= ~EPOLLOUT;
	}

	return errno;
}

static int epoll_recv(epoll_context* ctx)
{
	int r;
	r = recv(ctx->socket, ctx->in.recv.buffer, ctx->in.recv.bytes, 0);
	if(r >= 0)
		ctx->in.recv.proc(ctx->in.recv.param, 0, r);

	return r;
}

int aio_socket_recv(aio_socket_t socket, void* buffer, int bytes, aio_onrecv proc, void* param)
{
	epoll_context* ctx = (epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLIN));
	assert(0 == ctx->action_in);	
	if(0 != ctx->action_in)
		return EIO;

	ctx->in.recv.proc = proc;
	ctx->in.recv.param = param;
	ctx->in.recv.buffer = buffer;
	ctx->in.recv.bytes = bytes;

	if(0 <= epoll_recv(ctx))
		return 0;

	if(EAGAIN == errno)
	{
		ctx->action_in = epoll_recv;
		ctx->ev.events |= EPOLLIN;
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action_in = 0;
		ctx->ev.events &= ~EPOLLIN;
	}

	return errno;
}

static int epoll_send_v(epoll_context* ctx)
{
	int r;
	struct msghdr msg;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = (struct iovec*)ctx->out.send_v.vec;
	msg.msg_iovlen = ctx->out.send_v.n;

	r = sendmsg(ctx->socket, &msg, 0);
	if(r >= 0)
		ctx->out.send_v.proc(ctx->out.send_v.param, 0, r);

	return r;
}

int aio_socket_send_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onsend proc, void* param)
{
	int r;
	
	epoll_context* ctx = (epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLOUT));
	assert(0 == ctx->action_out);	
	if(0 != ctx->action_out)
		return EIO;

	ctx->out.send_v.proc = proc;
	ctx->out.send_v.param = param;
	ctx->out.send_v.vec = vec;
	ctx->out.send_v.n = n;

	if(0 <= epoll_send_v(ctx))
		return 0;

	if(EAGAIN == errno)
	{
		ctx->action_out = epoll_send_v;
		ctx->ev.events |= EPOLLOUT;
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action_out = 0;
		ctx->ev.events &= ~EPOLLOUT;
	}

	return errno;
}

static int epoll_recv_v(epoll_context* ctx)
{
	int r;
	struct msghdr msg;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = (struct iovec*)ctx->out.send_v.vec;
	msg.msg_iovlen = ctx->out.send_v.n;

	r = recvmsg(ctx->socket, &msg, 0);
	if(r >= 0)
		ctx->in.recv_v.proc(ctx->in.recv_v.param, 0, r);

	return r;
}

int aio_socket_recv_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecv proc, void* param)
{
	int r;
	epoll_context* ctx = (epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLIN));
	assert(0 == ctx->action_in);	
	if(0 != ctx->action_in)
		return EIO;

	ctx->in.recv_v.proc = proc;
	ctx->in.recv_v.param = param;
	ctx->in.recv_v.vec = vec;
	ctx->in.recv_v.n = n;

	if(0 <= epoll_recv_v(ctx))
		return 0;

	if(EAGAIN == errno)
	{
		ctx->action_in = epoll_recv_v;
		ctx->ev.events |= EPOLLIN;
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action_in = 0;
		ctx->ev.events &= ~EPOLLIN;
	}

	return errno;
}
