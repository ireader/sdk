#include "aio-socket.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <pthread.h>

//#define MAX_EVENT 64

// http://linux.die.net/man/2/epoll_wait see Notes
// For a discussion of what may happen if a file descriptor in an epoll instance being monitored by epoll_wait() is closed in another thread, see select(2). 
//
// http://linux.die.net/man/2/select
// Multithreaded applications
// If a file descriptor being monitored by select() is closed in another thread, the result is unspecified. 
// On some UNIX systems, select() unblocks and returns, with an indication that the file descriptor is ready 
// (a subsequent I/O operation will likely fail with an error, unless another the file descriptor reopened 
// between the time select() returned and the I/O operations was performed). 
// On Linux (and some other systems), closing the file descriptor in another thread has no effect on select(). 
// In summary, any application that relies on a particular behavior in this scenario must be considered buggy. 

// aio_socket_process(socket)
// 1. socket_setrecvtimeout(socket, xxx) failed, don't callback
// 2. socket_shutdown(socket, SHUT_WR) OK, call write callback (EPOLLOUT)
// 3. socket_shutdown(socket, SHUT_RD) OK, call read callback (EPOLLHUP|EPOLLIN)
// 4. socket_close(socket) failed, don't callback

// SIGPIPE
// 1. send after shutdown(SHUT_WR)

static int s_epoll = -1;
static int s_threads = 0;

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
	size_t bytes;
};

struct epoll_context_send
{
	aio_onsend proc;
	void *param;
	const void *buffer;
	size_t bytes;
	struct sockaddr_in addr; // for send to
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
	struct sockaddr_in addr;  // for send to
};

struct epoll_context_recvfrom
{
	aio_onrecvfrom proc;
	void *param;
	void *buffer;
	size_t bytes;
};

struct epoll_context_recvfrom_v
{
	aio_onrecvfrom proc;
	void *param;
	socket_bufvec_t *vec;
	int n;
};

struct epoll_context
{
	pthread_spinlock_t locker; // memory alignment, see more about Apple Developer spinlock
	struct epoll_event ev;
	socket_t socket;
	volatile int32_t ref;
	int own;

	int (*read)(struct epoll_context *ctx, int flags, int code);
	int (*write)(struct epoll_context *ctx, int flags, int code);

	union
	{
		struct epoll_context_accept accept;
		struct epoll_context_recv recv;
		struct epoll_context_recv_v recv_v;
		struct epoll_context_recvfrom recvfrom;
		struct epoll_context_recvfrom_v recvfrom_v;
	} in;

	union
	{
		struct epoll_context_connect connect;
		struct epoll_context_send send;
		struct epoll_context_send_v send_v;
	} out;
};

#define EPollIn(ctx, callback)   do {\
	ctx->read = callback;   \
	__sync_add_and_fetch_4(&ctx->ref, 1);	\
	pthread_spin_lock(&ctx->locker); ctx->ev.events |= EPOLLIN; pthread_spin_unlock(&ctx->locker); \
	if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))    \
		return 0;   \
	pthread_spin_lock(&ctx->locker); ctx->ev.events &= ~EPOLLIN; pthread_spin_unlock(&ctx->locker); \
	__sync_sub_and_fetch_4(&ctx->ref, 1);	\
} while(0)

#define EPollOut(ctx, callback)  do {\
	ctx->write = callback;         \
	__sync_add_and_fetch_4(&ctx->ref, 1);	\
	pthread_spin_lock(&ctx->locker); ctx->ev.events |= EPOLLOUT; pthread_spin_unlock(&ctx->locker); \
	if(0 == epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev))  \
		return 0;   \
	pthread_spin_lock(&ctx->locker); ctx->ev.events &= ~EPOLLOUT; pthread_spin_unlock(&ctx->locker); \
	__sync_sub_and_fetch_4(&ctx->ref, 1);	\
} while(0)

static int aio_socket_release(struct epoll_context* ctx)
{
	if( 0 == __sync_sub_and_fetch_4(&ctx->ref, 1) )
	{
		pthread_spin_destroy(&ctx->locker);

#if defined(DEBUG) || defined(_DEBUG)
		memset(ctx, 0xCC, sizeof(*ctx));
#endif
		free(ctx);
	}
	return 0;
}

int aio_socket_init(int threads)
{
	s_threads = threads;
	s_epoll = epoll_create(64);
	return -1 == s_epoll ? errno : 0;
}

int aio_socket_clean(void)
{
	if(-1 != s_epoll)
		close(s_epoll);
	return 0;
}

int aio_socket_process(int timeout)
{
	int i, r;
	uint32_t userevent;
	struct epoll_context* ctx;
	struct epoll_event events[1];

	r = epoll_wait(s_epoll, events, 1, timeout);
	for(i = 0; i < r; i++)
	{
		assert(events[i].data.ptr);
		ctx = (struct epoll_context*)events[i].data.ptr;
		// EPOLLERR: Error condition happened on the associated file descriptor
		// EPOLLHUP: Hang up happened on the associated file descriptor
		// EPOLLRDHUP: Stream socket peer closed connection, or shut down writing half of connection. 
		//			   (This flag is especially useful for writing simple code to detect peer shutdown when using Edge Triggered monitoring.) 
		if(events[i].events & (EPOLLERR|EPOLLHUP|EPOLLRDHUP))
		{
			// save event
			// 1. thread-1 current ctx->ev.events = EPOLLIN
			// 2. thread-2 user call aio_socket_send() set ctx->ev.events to EPOLLIN|EPOLLOUT
			// 3. switch thread-1 call ctx->write and decrement ctx->ref
			// 4. switch thread-2 aio_socket_send() epoll_ctl failed, then user set ctx->ev.events to EPOLLIN 
			// 5. thread-2 redo decrement ctx->ref (decrement twice, crash)
			pthread_spin_lock(&ctx->locker);
			userevent = ctx->ev.events;
			ctx->ev.events &= ~(EPOLLIN|EPOLLOUT);
			pthread_spin_unlock(&ctx->locker);
			epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev); // clear user event

			// error
			if(EPOLLIN & userevent)
			{
				assert(ctx->read);
				ctx->read(ctx, 1, EPIPE); // EPOLLRDHUP ?
				aio_socket_release(ctx);
			}

			if(EPOLLOUT & userevent)
			{
				assert(ctx->write);
				ctx->write(ctx, 1, EPIPE); // EPOLLRDHUP ?
				aio_socket_release(ctx);
			}
		}
		else
		{
			// clear IN/OUT event
			pthread_spin_lock(&ctx->locker);
			assert(events[i].events == (events[i].events & ctx->ev.events));
			ctx->ev.events &= ~(events[i].events & (EPOLLIN|EPOLLOUT));
			pthread_spin_unlock(&ctx->locker);
			if(ctx->ev.events & (EPOLLIN|EPOLLOUT))
				epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev);

			//if(EPOLLRDHUP & events[i].events)
			//{
			//	// closed
			//	assert(EPOLLIN & events[i].events);
			//}

			if(EPOLLIN & events[i].events)
			{
				assert(ctx->read);
				ctx->read(ctx, 1, 0);
				aio_socket_release(ctx);
			}

			if(EPOLLOUT & events[i].events)
			{
				assert(ctx->write);
				ctx->write(ctx, 1, 0);
				aio_socket_release(ctx);
			}
		}
	}

	return r;
}

aio_socket_t aio_socket_create(socket_t socket, int own)
{
	int flags;
	struct epoll_context* ctx;
	ctx = (struct epoll_context*)malloc(sizeof(struct epoll_context));
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(struct epoll_context));
	pthread_spin_init(&ctx->locker, PTHREAD_PROCESS_PRIVATE);
	ctx->own = own;
	ctx->ref = 1;
	ctx->socket = socket;
//	ctx->ev.events = EPOLLET|EPOLLRDHUP|EPOLLONESHOT; // Edge Triggered, for multi-thread epoll_wait(see more at epoll-wait-multithread.c)
	ctx->ev.events = EPOLLRDHUP|EPOLLONESHOT;
	ctx->ev.data.ptr = ctx;

	if(0 != epoll_ctl(s_epoll, EPOLL_CTL_ADD, socket, &ctx->ev))
	{
		free(ctx);
		return NULL;
	}

	// set non-blocking socket, for Edge Triggered
	flags = fcntl(socket, F_GETFL, 0);
	fcntl(socket, F_SETFL, flags | O_NONBLOCK);

	return ctx;
}

int aio_socket_destroy(aio_socket_t socket)
{
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(ctx->ev.data.ptr == ctx);

	if(0 != epoll_ctl(s_epoll, EPOLL_CTL_DEL, ctx->socket, &ctx->ev))
	{
		assert(EBADF == errno); // EBADF: socket close by user
//		return errno;
	}

	if(ctx->own && EBADF != errno)
	{
		shutdown(ctx->socket, SHUT_RDWR);
		close(ctx->socket);
	}

	ctx->socket = -1;

	aio_socket_release(ctx);
	return 0;
}

static int epoll_accept(struct epoll_context* ctx, int flags, int error)
{
	char ip[16];
	socket_t client;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	if(0 != error)
	{
		assert(1 == flags); // only in epoll_wait thread
		ctx->in.accept.proc(ctx->in.accept.param, error, 0, "", 0);
		return error;
	}

	memset(&addr, 0, sizeof(addr));
	client = accept(ctx->socket, (struct sockaddr*)&addr, &addrlen);
	if(client > 0)
	{
		strcpy(ip, inet_ntoa(addr.sin_addr));
		ctx->in.accept.proc(ctx->in.accept.param, 0, client, ip, ntohs(addr.sin_port));
		return 0;
	}
	else
	{
		assert(-1 == client);
		if(0 == flags)
			return errno;

		// call in epoll_wait thread
		ctx->in.accept.proc(ctx->in.accept.param, errno, 0, "", 0);
		return 0;
	}
}

int aio_socket_accept(aio_socket_t socket, aio_onaccept proc, void* param)
{
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLIN));
	if(ctx->ev.events & EPOLLIN)
		return EBUSY;

	ctx->in.accept.proc = proc;
	ctx->in.accept.param = param;

//	r = epoll_accept(ctx, 0, 0);
//	if(EAGAIN != r) return r;
	
	// man 2 accept to see more
	EPollIn(ctx, epoll_accept);
	return errno; // epoll_ctl return -1
}

static int epoll_connect(struct epoll_context* ctx, int flags, int error)
{
	int r;
	int addrlen = sizeof(ctx->out.connect.addr);

	if(0 != error)
	{
		assert(1 == flags); // only in epoll_wait thread
		ctx->out.connect.proc(ctx->out.connect.param, error);
		return error;
	}

	r = connect(ctx->socket, (const struct sockaddr*)&ctx->out.connect.addr, addrlen);
	if(0 == r)
	{
		// man connect to see more (EINPROGRESS)
		addrlen = sizeof(r);
		getsockopt(ctx->socket, SOL_SOCKET, SO_ERROR, (void*)&r, (socklen_t*)&addrlen);

		ctx->out.connect.proc(ctx->out.connect.param, r);
		return r;
	}
	else
	{
		if(0 == flags)
			return errno;

		// call in epoll_wait thread
		ctx->out.connect.proc(ctx->out.connect.param, errno);
		return 0;
	}
}

int aio_socket_connect(aio_socket_t socket, const char* ip, int port, aio_onconnect proc, void* param)
{
	int r;
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLOUT));
	if(ctx->ev.events & EPOLLOUT)
		return EBUSY;

	ctx->out.connect.addr.sin_family = AF_INET;
	ctx->out.connect.addr.sin_port = htons(port);
	ctx->out.connect.addr.sin_addr.s_addr = inet_addr(ip);
	ctx->out.connect.proc = proc;
	ctx->out.connect.param = param;

	r = epoll_connect(ctx, 0, 0);
	if(EINPROGRESS != r) return r;

	// man 2 connect to see more(ERRORS: EINPROGRESS)
	EPollOut(ctx, epoll_connect);
	return errno; // epoll_ctl return -1
}

static int epoll_recv(struct epoll_context* ctx, int flags, int error)
{
	ssize_t r;
	if(0 != error)
	{
		assert(1 == flags); // only in epoll_wait thread
		ctx->in.recv.proc(ctx->in.recv.param, error, 0);
		return error;
	}

	r = recv(ctx->socket, ctx->in.recv.buffer, ctx->in.recv.bytes, 0);
	if(r >= 0)
	{
		ctx->in.recv.proc(ctx->in.recv.param, 0, (size_t)r);
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		// call in epoll_wait thread
		ctx->in.recv.proc(ctx->in.recv.param, errno, 0);
		return 0;
	}
}

int aio_socket_recv(aio_socket_t socket, void* buffer, size_t bytes, aio_onrecv proc, void* param)
{
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLIN));
	if(ctx->ev.events & EPOLLIN)
		return EBUSY;

	ctx->in.recv.proc = proc;
	ctx->in.recv.param = param;
	ctx->in.recv.buffer = buffer;
	ctx->in.recv.bytes = bytes;

//	r = epoll_recv(ctx, 0, 0);
//	if(EAGAIN != r) return r;

	EPollIn(ctx, epoll_recv);
	return errno; // epoll_ctl return -1
}

static int epoll_send(struct epoll_context* ctx, int flags, int error)
{
	ssize_t r;
	if(0 != error)
	{
		assert(1 == flags); // only in epoll_wait thread
		ctx->out.send.proc(ctx->out.send.param, error, 0);
		return error;
	}

	r = send(ctx->socket, ctx->out.send.buffer, ctx->out.send.bytes, 0);
	if(r >= 0)
	{
		ctx->out.send.proc(ctx->out.send.param, 0, (size_t)r);
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		// call in epoll_wait thread
		ctx->out.send.proc(ctx->out.send.param, errno, 0);
		return 0;
	}
}

int aio_socket_send(aio_socket_t socket, const void* buffer, size_t bytes, aio_onsend proc, void* param)
{
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLOUT));
	if(ctx->ev.events & EPOLLOUT)
		return EBUSY;

	ctx->out.send.proc = proc;
	ctx->out.send.param = param;
	ctx->out.send.buffer = buffer;
	ctx->out.send.bytes = bytes;

//	r = epoll_send(ctx, 0, 0);
//	if(EAGAIN != r) return r;

	EPollOut(ctx, epoll_send);
	return errno; // epoll_ctl return -1
}

static int epoll_recv_v(struct epoll_context* ctx, int flags, int error)
{
	ssize_t r;
	struct msghdr msg;

	if(0 != error)
	{
		assert(1 == flags); // only in epoll_wait thread
		ctx->in.recv_v.proc(ctx->in.recv_v.param, error, 0);
		return error;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = (struct iovec*)ctx->in.recv_v.vec;
	msg.msg_iovlen = ctx->in.recv_v.n;

	r = recvmsg(ctx->socket, &msg, 0);
	if(r >= 0)
	{
		ctx->in.recv_v.proc(ctx->in.recv_v.param, 0, (size_t)r);
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		// call in epoll_wait thread
		ctx->in.recv_v.proc(ctx->in.recv_v.param, errno, 0);
		return 0;
	}
}

int aio_socket_recv_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecv proc, void* param)
{
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLIN));
	if(ctx->ev.events & EPOLLIN)
		return EBUSY;

	ctx->in.recv_v.proc = proc;
	ctx->in.recv_v.param = param;
	ctx->in.recv_v.vec = vec;
	ctx->in.recv_v.n = n;

//	r = epoll_recv_v(ctx, 0, 0);
//	if(EAGAIN != r) return r;

	EPollIn(ctx, epoll_recv_v);
	return errno; // epoll_ctl return -1
}

static int epoll_send_v(struct epoll_context* ctx, int flags, int error)
{
	ssize_t r;
	struct msghdr msg;

	if(0 != error)
	{
		assert(1 == flags); // only in epoll_wait thread
		ctx->out.send_v.proc(ctx->out.send_v.param, error, 0);
		return error;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = (struct iovec*)ctx->out.send_v.vec;
	msg.msg_iovlen = ctx->out.send_v.n;

	r = sendmsg(ctx->socket, &msg, 0);
	if(r >= 0)
	{
		ctx->out.send_v.proc(ctx->out.send_v.param, 0, (size_t)r);
        return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		// call in epoll_wait thread
		ctx->out.send_v.proc(ctx->out.send_v.param, errno, 0);
		return 0;
	}
}

int aio_socket_send_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onsend proc, void* param)
{
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLOUT));	
	if(ctx->ev.events & EPOLLOUT)
		return EBUSY;

	ctx->out.send_v.proc = proc;
	ctx->out.send_v.param = param;
	ctx->out.send_v.vec = vec;
	ctx->out.send_v.n = n;

//	r = epoll_send_v(ctx, 0, 0);
//	if(EAGAIN != r) return r;

	EPollOut(ctx, epoll_send_v);
	return errno; // epoll_ctl return -1
}

static int epoll_recvfrom(struct epoll_context* ctx, int flags, int error)
{
	ssize_t r;
	char ip[16] = {0};
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	if(0 != error)
	{
		assert(1 == flags); // only in epoll_wait thread
		ctx->in.recvfrom.proc(ctx->in.recvfrom.proc, error, 0, "", 0);
		return error;
	}

	memset(&addr.sin_addr, 0, sizeof(addr.sin_addr));
	r = recvfrom(ctx->socket, ctx->in.recvfrom.buffer, ctx->in.recvfrom.bytes, 0, (struct sockaddr*)&addr, &addrlen);
	if(r >= 0)
	{
		strcpy(ip, inet_ntoa(addr.sin_addr));
		ctx->in.recvfrom.proc(ctx->in.recvfrom.param, 0, (size_t)r, ip, (int)ntohs(addr.sin_port));
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		// call in epoll_wait thread
		ctx->in.recvfrom.proc(ctx->in.recvfrom.proc, errno, 0, "", 0);
		return 0;
	}
}

int aio_socket_recvfrom(aio_socket_t socket, void* buffer, size_t bytes, aio_onrecvfrom proc, void* param)
{
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLIN));	
	if(ctx->ev.events & EPOLLIN)
		return EBUSY;

	ctx->in.recvfrom.proc = proc;
	ctx->in.recvfrom.param = param;
	ctx->in.recvfrom.buffer = buffer;
	ctx->in.recvfrom.bytes = bytes;

//	r = epoll_recvfrom(ctx, 0, 0);
//	if(EAGAIN != r) return r;

	EPollIn(ctx, epoll_recvfrom);
	return errno; // epoll_ctl return -1
}

static int epoll_sendto(struct epoll_context* ctx, int flags, int error)
{
	ssize_t r;
	if(0 != error)
	{
		assert(1 == flags); // only in epoll_wait thread
		ctx->out.send.proc(ctx->out.send.param, error, 0);
		return error;
	}

	r = sendto(ctx->socket, ctx->out.send.buffer, ctx->out.send.bytes, 0, (struct sockaddr*)&ctx->out.send.addr, sizeof(ctx->out.send.addr));
	if(r >= 0)
	{
		ctx->out.send.proc(ctx->out.send.param, 0, (size_t)r);
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		// call in epoll_wait thread
		ctx->out.send.proc(ctx->out.send.param, errno, 0);
		return 0;
	}
}

int aio_socket_sendto(aio_socket_t socket, const char* ip, int port, const void* buffer, size_t bytes, aio_onsend proc, void* param)
{
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLOUT));
	if(ctx->ev.events & EPOLLOUT)
		return EBUSY;

	ctx->out.send.addr.sin_family = AF_INET;
	ctx->out.send.addr.sin_port = htons(port);
	ctx->out.send.addr.sin_addr.s_addr = inet_addr(ip);
	ctx->out.send.proc = proc;
	ctx->out.send.param = param;
	ctx->out.send.buffer = buffer;
	ctx->out.send.bytes = bytes;

//	r = epoll_sendto(ctx, 0, 0);
//	if(EAGAIN != r) return r;

	EPollOut(ctx, epoll_sendto);
	return errno; // epoll_ctl return -1
}

static int epoll_recvfrom_v(struct epoll_context* ctx, int flags, int error)
{
	ssize_t r;
	char ip[16] = {0};
	struct msghdr msg;
	struct sockaddr_in addr;

	if(0 != error)
	{
		assert(1 == flags); // only in epoll_wait thread
		ctx->in.recvfrom_v.proc(ctx->in.recvfrom_v.param, error, 0, "", 0);
		return error;
	}

	memset(&addr.sin_addr, 0, sizeof(addr.sin_addr));
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = (struct iovec*)ctx->in.recvfrom_v.vec;
	msg.msg_iovlen = ctx->in.recvfrom_v.n;

	r = recvmsg(ctx->socket, &msg, 0);
	if(r >= 0)
	{
		strcpy(ip, inet_ntoa(addr.sin_addr));
		ctx->in.recvfrom_v.proc(ctx->in.recvfrom_v.param, 0, (size_t)r, ip, (int)ntohs(addr.sin_port));
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		// call in epoll_wait thread
		ctx->in.recvfrom_v.proc(ctx->in.recvfrom_v.param, errno, 0, "", 0);
		return 0;
	}
}

int aio_socket_recvfrom_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecvfrom proc, void* param)
{
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLIN));
	if(ctx->ev.events & EPOLLIN)
		return EBUSY;

	ctx->in.recvfrom_v.proc = proc;
	ctx->in.recvfrom_v.param = param;
	ctx->in.recvfrom_v.vec = vec;
	ctx->in.recvfrom_v.n = n;

//	r = epoll_recvfrom_v(ctx, 0, 0);
//	if(EAGAIN != r) return r;

	EPollIn(ctx, epoll_recvfrom_v);
	return errno; // epoll_ctl return -1
}

static int epoll_sendto_v(struct epoll_context* ctx, int flags, int error)
{
	ssize_t r;
	struct msghdr msg;

	if(0 != error)
	{
		assert(1 == flags); // only in epoll_wait thread
		ctx->out.send_v.proc(ctx->out.send_v.param, error, 0);
		return error;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (struct sockaddr*)&ctx->out.send_v.addr;
	msg.msg_namelen = sizeof(ctx->out.send_v.addr);
	msg.msg_iov = (struct iovec*)ctx->out.send_v.vec;
	msg.msg_iovlen = ctx->out.send_v.n;

	r = sendmsg(ctx->socket, &msg, 0);
	if(r >= 0)
	{
		ctx->out.send_v.proc(ctx->out.send_v.param, 0, (size_t)r);
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		// call in epoll_wait thread
		ctx->out.send_v.proc(ctx->out.send_v.param, errno, 0);
		return 0;
	}
}

int aio_socket_sendto_v(aio_socket_t socket, const char* ip, int port, socket_bufvec_t* vec, int n, aio_onsend proc, void* param)
{
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLOUT));	
	if(ctx->ev.events & EPOLLOUT)
		return EBUSY;

	ctx->out.send_v.addr.sin_family = AF_INET;
	ctx->out.send_v.addr.sin_port = htons(port);
	ctx->out.send_v.addr.sin_addr.s_addr = inet_addr(ip);
	ctx->out.send_v.proc = proc;
	ctx->out.send_v.param = param;
	ctx->out.send_v.vec = vec;
	ctx->out.send_v.n = n;

//	r = epoll_sendto_v(ctx, 0, 0);
//	if(EAGAIN != r) return r;

	EPollOut(ctx, epoll_sendto_v);
	return errno; // epoll_ctl return -1
}
