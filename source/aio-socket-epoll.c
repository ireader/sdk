#include "aio-socket.h"
#include "thread-pool.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

static int s_epoll = -1;

enum epoll_action {
	EPOLL_NONE			= 0,

	// EPOLLIN
	EPOLL_ACCEPT		= 0x01,
	EPOLL_RECV			= 0x02,
	EPOLL_RECV_V		= 0x03,
	EPOLL_RECVFROM		= 0x04,
	EPOLL_RECVFROM_V	= 0x05,

	// EPOLLOUT
	EPOLL_CONNECT		= 0x01,
	EPOLL_SEND			= 0x02,
	EPOLL_SEND_V		= 0x03,
	EPOLL_SENDTO		= 0x04,
	EPOLL_SENDTO_V		= 0x05,
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
	int action_in;
	int action_out;

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

static int epoll_action_accept(epoll_context* ctx)
{
	char ip[16];
	socket_t client;
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);

	client = accept(ctx->socket, &addr, &addrlen);
	if(client > 0)
	{
		sprintf(ip, "%d.%d.%d.%d", addr.sin_addr.s_net, addr.sin_addr.s_host, addr.sin_addr.s_lh, addr.sin_addr.s_impno);
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

static int epoll_process()
{
	int i, r;
	epoll_context* ctx;
	struct epll_event events[MAX_EVENT];

	r = epoll_wait(s_epoll, events, MAX_EVENT, 2000);
	for(i = 0; i < r; i++)
	{
		ctx = (epoll_context*)events[i].data.ptr;

		if(EPOLLIN & events[i].events)
		{			
			switch(ctx->action_in)
			{
			case EPOLL_ACCEPT:
				break;

			case EPOLL_RECV:
				break;

			case EPOLL_RECV_V:
				break;

			case EPOLL_RECVFROM:
				break;

			case EPOLL_RECVFROM_V:
				break;

			default:
				assert(false);
			}

			ctx->action_in = 0;
			ctx->ev.events &= ~EPOLLIN;
		}
		
		if(EPOLLOUT & events[i].events)
		{
			switch(ctx->action_out)
			{
			case EPOLL_CONNECT:
				break;

			case EPOLL_SEND:
				break;

			case EPOLL_SEND_V:
				break;

			case EPOLL_SENDTO:
				break;

			case EPOLL_SENDTO_V:
				break;

			default:
				assert(false);
			}

			ctx->action_out = 0;
			ctx->ev.events &= ~EPOLLOUT;
		}
	}
}

int aio_socket_init()
{
	s_epoll = epoll_create(INT_MAX);
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

int aio_socket_accept(aio_socket_t socket, const char* ip, int port, aio_onaccept proc, void* param)
{
	int r;
	char ip[16];
	socket_t client;
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	epoll_context* ctx = (epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLIN));
	assert(0 == ctx->action_in);
	if(0 != ctx->action_in)
		return EIO;

	if(!proc)
		return ENOTEMPTY;

	ctx->in.accept.proc = proc;
	ctx->in.accept.param = param;
	if(0 == epoll_action_accept(ctx))
		return 0;

	assert(-1 == client);
	if(EAGAIN == errno)
	{
		ctx->action_in = EPOLL_ACCEPT;
		ctx->ev.events |= EPOLLIN; // man 2 accept to see more
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action_in = 0;
		ctx->ev.events &= ~EPOLLIN;
	}

	return errno;
}

int aio_socket_connect(aio_socket_t socket, const char* ip, int port, aio_onconnect proc, void* param)
{
	int r;
	char ip[16];
	socket_t client;
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	epoll_context* ctx = (epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLOUT));
	assert(0 == ctx->action_out);	
	if(0 != ctx->action_out)
		return EIO;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	r = connect(ctx->socket, &addr, &addrlen);
	if(0 == r)
	{
		addrlen = sizeof(r);
		getsockopt(ctx->socket, SOL_SOCKET, SO_ERROR, (void*)&r, (socklen_t*)&addrlen);

		sprintf(ip, "%d.%d.%d.%d", addr.sin_addr.s_net, addr.sin_addr.s_host, addr.sin_addr.s_lh, addr.sin_addr.s_impno);
		proc(param, r, client, ip, ntohs(addr.sin_port));
		return r;
	}

	if(EINPROGRESS == errno)
	{
		ctx->out.connect.proc = proc;
		ctx->out.connect.param = param;
		ctx->action_out = EPOLL_CONNECT;
		ctx->ev.events |= EPOLLOUT; // man 2 connect to see more(ERRORS: EINPROGRESS)
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action_out = 0;
		ctx->ev.events &= ~EPOLLOUT;
	}

	return errno;
}

int aio_socket_send(aio_socket_t socket, const void* buffer, int bytes, aio_onsend proc, void* param)
{
	int r;
	epoll_context* ctx = (epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLOUT));
	assert(0 == ctx->action_out);	
	if(0 != ctx->action_out)
		return EIO;

	r = send(ctx->socket, buffer, bytes, 0);
	if(-1 != r)
	{
		proc(param, 0, r);
		return 0;
	}

	if(EAGAIN == errno)
	{
		ctx->out.send.proc = proc;
		ctx->out.send.param = param;
		ctx->out.send.buffer = buffer;
		ctx->out.send.bytes = bytes;
		ctx->action_out = EPOLL_SEND;
		ctx->ev.events |= EPOLLOUT;
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action_out = 0;
		ctx->ev.events &= ~EPOLLOUT;
	}

	return errno;
}

int aio_socket_recv(aio_socket_t socket, void* buffer, int bytes, aio_onrecv proc, void* param)
{
	int r;
	epoll_context* ctx = (epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLIN));
	assert(0 == ctx->action_in);	
	if(0 != ctx->action_in)
		return EIO;

	r = recv(ctx->socket, buffer, bytes, 0);
	if(-1 != r)
	{
		proc(param, 0, r);
		return 0;
	}

	if(EAGAIN == errno)
	{
		ctx->in.recv.proc = proc;
		ctx->in.recv.param = param;
		ctx->in.recv.buffer = buffer;
		ctx->in.recv.bytes = bytes;
		ctx->action_in = EPOLL_RECV;
		ctx->ev.events |= EPOLLIN;
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action_in = 0;
		ctx->ev.events &= ~EPOLLIN;
	}

	return errno;
}

int aio_socket_send_v(aio_socket_t socket, const socket_bufvec_t* vec, size_t n, aio_onsend proc, void* param)
{
	int r;
	struct msghdr msg;
	epoll_context* ctx = (epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLOUT));
	assert(0 == ctx->action_out);	
	if(0 != ctx->action_out)
		return EIO;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = (struct iovec*)vec;
	msg.msg_iovlen = n;

	r = sendmsg(ctx->socket, &msg, 0);
	if(-1 != r)
	{
		proc(param, 0, r);
		return 0;
	}

	if(EAGAIN == errno)
	{
		ctx->out.send_v.proc = proc;
		ctx->out.send_v.param = param;
		ctx->out.send_v.vec = vec;
		ctx->out.send_v.n = n;
		ctx->action_out = EPOLL_SEND_V;
		ctx->ev.events |= EPOLLOUT;
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action_out = 0;
		ctx->ev.events &= ~EPOLLOUT;
	}

	return errno;
}

int aio_socket_recv_v(aio_socket_t socket, socket_bufvec_t* vec, size_t n, aio_onrecv proc, void* param)
{
	int r;
	struct msghdr msg;
	epoll_context* ctx = (epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLIN));
	assert(0 == ctx->action_in);	
	if(0 != ctx->action_in)
		return EIO;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = vec;
	msg.msg_iovlen = n;

	r = recvmsg(ctx->socket, &msg, 0);
	if(-1 != r)
	{
		proc(param, 0, r);
		return 0;
	}

	if(EAGAIN == errno)
	{
		ctx->in.recv_v.proc = proc;
		ctx->in.recv_v.param = param;
		ctx->in.recv_v.vec = vec;
		ctx->in.recv_v.n = n;
		ctx->action_in = EPOLL_RECV_V;
		ctx->ev.events |= EPOLLIN;
		if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))
			return 0;

		ctx->action_in = 0;
		ctx->ev.events &= ~EPOLLIN;
	}

	return errno;
}
