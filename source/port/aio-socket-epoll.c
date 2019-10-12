#include "aio-socket.h"
#include "sys/spinlock.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

#ifndef EPOLLONESHOT
#define EPOLLONESHOT 0x40000000
#endif

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
	struct sockaddr_storage addr;  // for send to
	socklen_t addrlen;
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
	struct sockaddr_storage addr;  // for send to
	socklen_t addrlen;
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
	struct sockaddr_storage addr;  // for send to
	socklen_t addrlen;
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
	spinlock_t locker; // memory alignment, see more about Apple Developer spinlock
	struct epoll_event ev;
	socket_t socket;
	volatile int32_t ref;
	int own;
	int init; // epoll_ctl add

	aio_ondestroy ondestroy;
	void* param;

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

#define EPollCtrl(ctx, flag) do {				\
	int r;										\
	__sync_add_and_fetch_4(&ctx->ref, 1);		\
	spinlock_lock(&ctx->locker);			\
	ctx->ev.events |= flag;						\
	if(0 == ctx->init)							\
	{											\
		r = epoll_ctl(s_epoll, EPOLL_CTL_ADD, ctx->socket, &ctx->ev);	\
		ctx->init = (0 == r ? 1 : 0);			\
	}											\
	else										\
	{											\
		r = epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev);	\
	}											\
	if(0 != r)									\
	{											\
		ctx->ev.events &= ~flag;				\
		__sync_sub_and_fetch_4(&ctx->ref, 1);	\
	}											\
	spinlock_unlock(&ctx->locker);			\
	if(0 == r) return 0;						\
} while(0)

#define EPollIn(ctx, callback)	ctx->read = callback; EPollCtrl(ctx, EPOLLIN)
#define EPollOut(ctx, callback)	ctx->write = callback; EPollCtrl(ctx, EPOLLOUT)

static int aio_socket_release(struct epoll_context* ctx)
{
	if( 0 == __sync_sub_and_fetch_4(&ctx->ref, 1) )
	{
		if(0 != ctx->init && 0 != epoll_ctl(s_epoll, EPOLL_CTL_DEL, ctx->socket, &ctx->ev))
		{
			assert(EBADF == errno); // EBADF: socket close by user
			//		return errno;
		}

		if(ctx->own)
			close(ctx->socket);

		spinlock_destroy(&ctx->locker);

		if (ctx->ondestroy)
			ctx->ondestroy(ctx->param);

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

	// Since Linux 2.6.8, the size argument is ignored, but must be greater than zero
	s_epoll = epoll_create(10000/*10k*/);
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
		// EPOLLERR: Error condition happened on the associated file descriptor
		// EPOLLHUP: Hang up happened on the associated file descriptor
		// EPOLLRDHUP: Stream socket peer closed connection, or shut down writing half of connection. 
		//			   (This flag is especially useful for writing simple code to detect peer shutdown when using Edge Triggered monitoring.) 
		int flags = EPOLLERR|EPOLLHUP;
#if defined(EPOLLRDHUP)
		flags |= EPOLLRDHUP;
#endif
		assert(events[i].data.ptr);
		ctx = (struct epoll_context*)events[i].data.ptr;
		assert(ctx->ref > 0);
		if(events[i].events & flags)
		{
			// save event
			spinlock_lock(&ctx->locker);
			userevent = ctx->ev.events;
			ctx->ev.events &= ~(EPOLLIN|EPOLLOUT);
			spinlock_unlock(&ctx->locker);

			// epoll oneshot don't need change event
			//if(userevent & (EPOLLIN|EPOLLOUT))
			//	epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev); // endless loop

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
			// save event
			// 1. thread-1 current ctx->ev.events = EPOLLIN
			// 2. thread-2 user call aio_socket_send() set ctx->ev.events to EPOLLIN|EPOLLOUT
			// 3. switch thread-1 call ctx->write and decrement ctx->ref
			// 4. switch thread-2 aio_socket_send() epoll_ctl failed, then user set ctx->ev.events to EPOLLIN 
			// 5. thread-2 redo decrement ctx->ref (decrement twice, crash)

			// clear IN/OUT event
			spinlock_lock(&ctx->locker);
	
			// 1. thread-1 aio_socket_send() set ctx->ev.events to EPOLLOUT
			// 2. thread-2 epoll_wait -> events[i].events EPOLLOUT
			// 3. thread-1 aio_socket_recv() set ctx->ev.events to EPOLLOUT|EPOLLIN
			// 4. thread-1 epoll_wait -> events[i].events EPOLLOUT
			// 5. thread-1 set ctx->ev.events to EPOLLIN
			// 6. thread-2 check ctx->ev.events with EPOLLOUT failed
			//assert(events[i].events == (events[i].events & ctx->ev.events));

			events[i].events &= ctx->ev.events; // check events again(multi-thread condition)
			ctx->ev.events &= ~(events[i].events & (EPOLLIN | EPOLLOUT));
			if(ctx->ev.events & (EPOLLIN|EPOLLOUT))
				epoll_ctl(s_epoll, EPOLL_CTL_MOD, ctx->socket, &ctx->ev); // update epoll event(clear in/out cause EPOLLHUP)
			spinlock_unlock(&ctx->locker);

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
//	int flags;
	struct epoll_context* ctx;
	ctx = (struct epoll_context*)calloc(1, sizeof(struct epoll_context));
	if(!ctx)
		return NULL;

	spinlock_create(&ctx->locker);
	ctx->own = own;
	ctx->ref = 1; // 1-for EPOLLHUP(no in/out, shutdown), 2-destroy release
	ctx->socket = socket;
//	ctx->ev.events |= EPOLLET; // Edge Triggered, for multi-thread epoll_wait(see more at epoll-wait-multithread.c)
	ctx->ev.events |= EPOLLONESHOT; // since Linux 2.6.2(include EPOLLWAKEUP|EPOLLONESHOT|EPOLLET, see: linux/fs/eventpoll.c)
#if defined(EPOLLRDHUP)
	ctx->ev.events |= EPOLLRDHUP; // since Linux 2.6.17
#endif
	ctx->ev.data.ptr = ctx;

	// don't add to epoll until read/write
	//if(0 != epoll_ctl(s_epoll, EPOLL_CTL_ADD, socket, &ctx->ev))
	//{
	//	free(ctx);
	//	return NULL;
	//}

	// set non-blocking socket, for Edge Triggered
	//flags = fcntl(socket, F_GETFL, 0);
	//fcntl(socket, F_SETFL, flags | O_NONBLOCK);

	return ctx;
}

int aio_socket_destroy(aio_socket_t socket, aio_ondestroy ondestroy, void* param)
{
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(ctx->ev.data.ptr == ctx);
	ctx->ondestroy = ondestroy;
	ctx->param = param;

	shutdown(ctx->socket, SHUT_RDWR);
//	close(sock); // can't close socket now, avoid socket reuse

	aio_socket_release(ctx); // shutdown will generate EPOLLHUP event
	return 0;
}

static int epoll_accept(struct epoll_context* ctx, int flags, int error)
{
	socket_t client;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);

	if(0 != error)
	{
		assert(1 == flags); // only in epoll_wait thread
		ctx->in.accept.proc(ctx->in.accept.param, error, 0, NULL, 0);
		return error;
	}

	client = accept(ctx->socket, (struct sockaddr*)&addr, &addrlen);
	if(client > 0)
	{
		ctx->in.accept.proc(ctx->in.accept.param, 0, client, (struct sockaddr*)&addr, addrlen);
		return 0;
	}
	else
	{
		assert(-1 == client);
		if(0 == flags)
			return errno;

		// call in epoll_wait thread
		ctx->in.accept.proc(ctx->in.accept.param, errno, 0, NULL, 0);
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
	socklen_t len;

    // call in epoll_wait thread
    assert(1 == flags);

    if(0 != error)
	{
		ctx->out.connect.proc(ctx->out.connect.param, error);
		return error;
	}
    else
    {
        // man connect to see more (EINPROGRESS)
		len = sizeof(error);
        getsockopt(ctx->socket, SOL_SOCKET, SO_ERROR, (void*)&error, &len);
        ctx->out.connect.proc(ctx->out.connect.param, error);
        return error;
    }
}

int aio_socket_connect(aio_socket_t socket, const struct sockaddr *addr, socklen_t addrlen, aio_onconnect proc, void* param)
{
	int r;
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLOUT));
	if(ctx->ev.events & EPOLLOUT)
		return EBUSY;

	ctx->out.connect.addrlen = addrlen > sizeof(ctx->out.connect.addr) ? sizeof(ctx->out.connect.addr) : addrlen;
	memcpy(&ctx->out.connect.addr, addr, ctx->out.connect.addrlen);
	ctx->out.connect.proc = proc;
	ctx->out.connect.param = param;

//	r = epoll_connect(ctx, 0, 0);
//	if(EINPROGRESS != r) return r;

    r = connect(ctx->socket, (const struct sockaddr*)&ctx->out.connect.addr, ctx->out.connect.addrlen);
    if(0 == r || EINPROGRESS == errno)
    {
        EPollOut(ctx, epoll_connect);
    }
	return errno; // epoll_ctl return -1
}

static int epoll_recv(struct epoll_context* ctx, int flags, int error)
{
	ssize_t r;

	// recv socket buffer data
	//if(0 != error)
	//{
	//	assert(1 == flags); // only in epoll_wait thread
	//	ctx->in.recv.proc(ctx->in.recv.param, error, 0);
	//	return error;
	//}

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

	// recv socket buffer data
	//if(0 != error)
	//{
	//	assert(1 == flags); // only in epoll_wait thread
	//	ctx->in.recv_v.proc(ctx->in.recv_v.param, error, 0);
	//	return error;
	//}

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
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);

	if(0 != error)
	{
		assert(1 == flags); // only in epoll_wait thread
		ctx->in.recvfrom.proc(ctx->in.recvfrom.param, error, 0, NULL, 0);
		return error;
	}

	r = recvfrom(ctx->socket, ctx->in.recvfrom.buffer, ctx->in.recvfrom.bytes, 0, (struct sockaddr*)&addr, &addrlen);
	if(r >= 0)
	{
		ctx->in.recvfrom.proc(ctx->in.recvfrom.param, 0, (size_t)r, (struct sockaddr*)&addr, addrlen);
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		// call in epoll_wait thread
		ctx->in.recvfrom.proc(ctx->in.recvfrom.param, errno, 0, NULL, 0);
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

	r = sendto(ctx->socket, ctx->out.send.buffer, ctx->out.send.bytes, 0, (struct sockaddr*)&ctx->out.send.addr, ctx->out.send.addrlen);
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

int aio_socket_sendto(aio_socket_t socket, const struct sockaddr *addr, socklen_t addrlen, const void* buffer, size_t bytes, aio_onsend proc, void* param)
{
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLOUT));
	if(ctx->ev.events & EPOLLOUT)
		return EBUSY;

	ctx->out.send.addrlen = addrlen > sizeof(ctx->out.send.addr) ? sizeof(ctx->out.send.addr) : addrlen;
	memcpy(&ctx->out.send.addr, addr, ctx->out.send.addrlen);
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
	struct sockaddr_storage addr;
	struct msghdr msg;

	if(0 != error)
	{
		assert(1 == flags); // only in epoll_wait thread
		ctx->in.recvfrom_v.proc(ctx->in.recvfrom_v.param, error, 0, NULL, 0);
		return error;
	}

	memset(&addr, 0, sizeof(addr));
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = (struct iovec*)ctx->in.recvfrom_v.vec;
	msg.msg_iovlen = ctx->in.recvfrom_v.n;

	r = recvmsg(ctx->socket, &msg, 0);
	if(r >= 0)
	{
		ctx->in.recvfrom_v.proc(ctx->in.recvfrom_v.param, 0, (size_t)r, (struct sockaddr*)&addr, msg.msg_namelen);
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		// call in epoll_wait thread
		ctx->in.recvfrom_v.proc(ctx->in.recvfrom_v.param, errno, 0, NULL, 0);
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
	msg.msg_namelen = ctx->out.send_v.addrlen;
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

int aio_socket_sendto_v(aio_socket_t socket, const struct sockaddr *addr, socklen_t addrlen, socket_bufvec_t* vec, int n, aio_onsend proc, void* param)
{
	struct epoll_context* ctx = (struct epoll_context*)socket;
	assert(0 == (ctx->ev.events & EPOLLOUT));	
	if(ctx->ev.events & EPOLLOUT)
		return EBUSY;

	ctx->out.send_v.addrlen = addrlen > sizeof(ctx->out.send_v.addr) ? sizeof(ctx->out.send_v.addr) : addrlen;
	memcpy(&ctx->out.send_v.addr, addr, ctx->out.send_v.addrlen);
	ctx->out.send_v.proc = proc;
	ctx->out.send_v.param = param;
	ctx->out.send_v.vec = vec;
	ctx->out.send_v.n = n;

//	r = epoll_sendto_v(ctx, 0, 0);
//	if(EAGAIN != r) return r;

	EPollOut(ctx, epoll_sendto_v);
	return errno; // epoll_ctl return -1
}
