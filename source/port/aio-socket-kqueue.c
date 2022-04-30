#if defined(OS_MAC)
#include "aio-socket.h"
#include "sys/atomic.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

//#define MAX_EVENT 64

// http://www.freebsd.org/cgi/man.cgi?query=kqueue&apropos=0&sektion=0&format=html see Notes
//
// Calling close() on a file descriptor	will remove any	kevents	that reference the descriptor.

static int s_kqueue = -1;

struct kqueue_context_accept
{
	aio_onaccept proc;
	void *param;
};

struct kqueue_context_connect
{
	aio_onconnect proc;
	void *param;
	struct sockaddr_storage addr;
	socklen_t addrlen;
};

struct kqueue_context_recv
{
	aio_onrecv proc;
	void *param;
	void *buffer;
	size_t bytes;
};

struct kqueue_context_send
{
	aio_onsend proc;
	void *param;
	const void *buffer;
	size_t bytes;
	struct sockaddr_storage addr; // for send to
	socklen_t addrlen;
};

struct kqueue_context_recv_v
{
	aio_onrecv proc;
	void *param;
	socket_bufvec_t *vec;
	int n;
};

struct kqueue_context_send_v
{
	aio_onsend proc;
	void *param;
	socket_bufvec_t *vec;
	int n;
	struct sockaddr_storage addr;  // for send to
	socklen_t addrlen;
};

struct kqueue_context_recvfrom
{
	aio_onrecvfrom proc;
	void *param;
	void *buffer;
	size_t bytes;
};

struct kqueue_context_recvfrom_v
{
	aio_onrecvfrom proc;
	void *param;
	socket_bufvec_t *vec;
	int n;
};

struct kqueue_context
{
	struct kevent ev[2]; // 0-read, 1-write
    socket_t socket[2];
    volatile int32_t ref;
    int own;
	//int ref;
	//int closed;
    
    aio_ondestroy ondestroy;
    void* param;

	int (*read)(struct kqueue_context *ctx, int flags, int code);
	int (*write)(struct kqueue_context *ctx, int flags, int code);

	union
	{
		struct kqueue_context_accept accept;
		struct kqueue_context_recv recv;
		struct kqueue_context_recv_v recv_v;
		struct kqueue_context_recvfrom recvfrom;
		struct kqueue_context_recvfrom_v recvfrom_v;
	} in;

	union
	{
		struct kqueue_context_connect connect;
		struct kqueue_context_send send;
		struct kqueue_context_send_v send_v;
	} out;
};

#define KQueueRead(ctx, callback)   do {\
    ctx->read = callback;   \
    atomic_increment32(&ctx->ref);        \
    EV_SET(&ctx->ev[0], ctx->socket[0], EVFILT_READ, EV_ADD|EV_ONESHOT, 0, 0, ctx);    \
    if(-1 != kevent(s_kqueue, &ctx->ev[0], 1, NULL, 0, NULL))   \
        return 0;   \
    ctx->ev[0].filter = 0; \
    atomic_decrement32(&ctx->ref);    \
} while(0)

#define KQueueWrite(ctx, callback)  do {\
    ctx->write = callback;         \
    atomic_increment32(&ctx->ref);        \
    EV_SET(&ctx->ev[1], ctx->socket[1], EVFILT_WRITE, EV_ADD|EV_ONESHOT, 0, 0, ctx);   \
    if(-1 != kevent(s_kqueue, &ctx->ev[1], 1, NULL, 0, NULL))   \
        return 0;   \
    ctx->ev[1].filter = 0;  \
    atomic_decrement32(&ctx->ref);    \
} while(0)

static int aio_socket_release(struct kqueue_context* ctx)
{
    if( 0 == atomic_decrement32(&ctx->ref) )
    {
        //EV_SET(&ctx->ev[0], ctx->socket, 0, EV_DELETE, 0, 0, ctx);
        //EV_SET(&ctx->ev[1], ctx->socket, 0, EV_DELETE, 0, 0, ctx);

        if(ctx->own)
            close(ctx->socket[0]);
        close(ctx->socket[1]);

        //spinlock_destroy(&ctx->locker);

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
    (void)threads;
	s_kqueue = kqueue();
	return -1 == s_kqueue ? errno : 0;
}

int aio_socket_clean(void)
{
	if(-1 != s_kqueue)
		close(s_kqueue);
	return 0;
}

int aio_socket_process(int timeout)
{
	int i, r;
	struct timespec ts;
	struct kevent events[1];
	struct kqueue_context *ctx;

	ts.tv_sec = timeout / 1000;
	ts.tv_nsec = (timeout % 1000) * 1000000;

	r = kevent(s_kqueue, NULL, 0, events, 1, &ts);
	for(i = 0; i < r; i++)
	{
		assert(events[i].udata);
		ctx = (struct kqueue_context*)events[i].udata;
		if(events[i].flags & EV_ERROR)
		{
			// error
			if(EVFILT_READ == ctx->ev[0].filter)
			{
                ctx->ev[0].filter = 0;
				assert(ctx->read);
				ctx->read(ctx, 1, -1);
                aio_socket_release(ctx);
			}

			if(EVFILT_WRITE == ctx->ev[1].filter)
			{
                ctx->ev[1].filter = 0;
                assert(ctx->write);
				ctx->write(ctx, 1, -1);
                aio_socket_release(ctx);
			}
		}
		else
		{
			if(EVFILT_READ == events[i].filter)
			{
                ctx->ev[0].filter = 0;
				// event[i].data

				// - listen: data contains the size of the listen backlog
				// - data contains the number of bytes of protocol data available to read.
				// - If the read direction of the socket has shutdown, then the filter also 
				//   sets EV_EOF in flags, and returns the socket error (if any) in fflags. 
				// - It is possible for EOF to be returned (indicating the connection is gone)
				//	 while there is still data pending in the socket buffer.
				assert(ctx->read);
				ctx->read(ctx, 1, 0);
                aio_socket_release(ctx);
			}

			if(EVFILT_WRITE == events[i].filter)
			{
                ctx->ev[1].filter = 0;
				// - For sockets, pipes and fifos, data will contain the amount of space
				//   remaining in the write buffer. 
				// - The filter will set EV_EOF when the reader disconnects, and for the fifo case,
				//   this may be cleared by use of EV_CLEAR.
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
	struct kqueue_context* ctx;
	ctx = (struct kqueue_context*)malloc(sizeof(struct kqueue_context));
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(struct kqueue_context));
	ctx->own = own;
    ctx->ref = 1; // 1-for EPOLLHUP(no in/out, shutdown), 2-destroy release
	ctx->socket[0] = socket;
    ctx->socket[1] = dup(socket);
    ctx->ev[0].udata = ctx;
    ctx->ev[1].udata = ctx;
//    EV_SET(&ctx->ev[0], ctx->socket, EVFILT_READ, EV_ADD|EV_ONESHOT, 0, 0, ctx);
//    EV_SET(&ctx->ev[1], ctx->socket, EVFILT_WRITE, EV_ADD|EV_ONESHOT, 0, 0, ctx);
//    if(-1 == kevent(s_kqueue, ctx->ev, 2, NULL, 0, NULL))
//    {
//        free(ctx);
//        return NULL;
//    }

	// set non-blocking socket, for Edge Triggered
//	flags = fcntl(socket[0], F_GETFL, 0);
//	fcntl(socket[0], F_SETFL, flags | O_NONBLOCK);
//    flags = fcntl(socket[1], F_GETFL, 0);
//    fcntl(socket[1], F_SETFL, flags | O_NONBLOCK);

	return ctx;
}

int aio_socket_destroy(aio_socket_t socket, aio_ondestroy ondestroy, void* param)
{
    struct kqueue_context* ctx = (struct kqueue_context*)socket;
    assert(ctx->ev[0].udata == ctx);
    assert(ctx->ev[1].udata == ctx);
    ctx->ondestroy = ondestroy;
    ctx->param = param;

//    EV_SET(&ctx->ev[0], ctx->socket, 0, EV_DELETE, 0, 0, ctx);
//    EV_SET(&ctx->ev[1], ctx->socket, 0, EV_DELETE, 0, 0, ctx);
//    if(-1 == kevent(s_kqueue, ctx->ev, 2, NULL, 0, NULL))
//    {
//		assert(0);
//		return errno;
//    }

	if(ctx->own)
		shutdown(ctx->socket[0], SHUT_RDWR);
    aio_socket_release(ctx); // shutdown will generate EPOLLHUP event
	return 0;
}

static int kqueue_accept(struct kqueue_context* ctx, int flags, int error)
{
	// data contains the size of the listen backlog
	socket_t client;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);

	if(0 != error)
	{
		assert(1 == flags); // only in kevent thread
		ctx->in.accept.proc(ctx->in.accept.param, error, 0, NULL, 0);
		return error;
	}

	client = accept(ctx->socket[0], (struct sockaddr*)&addr, &addrlen);
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

		ctx->in.accept.proc(ctx->in.accept.param, errno, 0, NULL, 0);
		return 0;
	}
}

int aio_socket_accept(aio_socket_t socket, aio_onaccept proc, void* param)
{
    int r = -1;
    struct kqueue_context* ctx = (struct kqueue_context*)socket;
	assert(0 == ctx->ev[0].filter);
	if(ctx->ev[0].filter)
		return EBUSY;

	ctx->in.accept.proc = proc;
	ctx->in.accept.param = param;

//    r = kqueue_accept(ctx, 0, 0);
//	if(EAGAIN == r)
	{
        KQueueRead(ctx, kqueue_accept);
	}

	return 0 == r ? 0 : errno;
}

static int kqueue_connect(struct kqueue_context* ctx, int flags, int error)
{
    int r = 0;
    socklen_t addrlen = sizeof(ctx->out.connect.addr);
    
    // call in kevent thread
    assert(1 == flags);
    
    if(0 != error)
    {
        assert(1 == flags); // only in kevent thread
        ctx->out.connect.proc(ctx->out.connect.param, error);
        return error;
    }
    else
    {
        // man connect to see more (EINPROGRESS)
        addrlen = sizeof(r);
        getsockopt(ctx->socket[1], SOL_SOCKET, SO_ERROR, (void*)&r, &addrlen);
        ctx->out.connect.proc(ctx->out.connect.param, r);
        return r;
    }
}

int aio_socket_connect(aio_socket_t socket, const struct sockaddr *addr, socklen_t addrlen, aio_onconnect proc, void* param)
{
    int r = -1;
    struct kqueue_context* ctx = (struct kqueue_context*)socket;
    assert(0 == ctx->ev[1].filter);
    if(ctx->ev[1].filter)
        return EBUSY;

	ctx->out.connect.addrlen = addrlen > sizeof(ctx->out.connect.addr) ? sizeof(ctx->out.connect.addr) : addrlen;
	memcpy(&ctx->out.connect.addr, addr, ctx->out.connect.addrlen);
	ctx->out.connect.proc = proc;
	ctx->out.connect.param = param;

    //r = kqueue_connect(ctx, 0, 0);
    addrlen = sizeof(ctx->out.connect.addr);
    r = connect(ctx->socket[1], (const struct sockaddr*)&ctx->out.connect.addr, ctx->out.connect.addrlen);
//    if(0 == r)
//    {
//        // man 2 connect to see more(ERRORS: EINPROGRESS)
//        addrlen = sizeof(r);
//        if(0 == getsockopt(ctx->socket[1], SOL_SOCKET, SO_ERROR, (void*)&r, &addrlen) && 0 == r)
//            ctx->out.connect.proc(ctx->out.connect.param, r);
//        return r;
//    }
//
//    if(EINPROGRESS == errno)
    if(0 == r || EINPROGRESS == errno)
    {
    	KQueueWrite(ctx, kqueue_connect);
	}

	return 0 == r ? 0 : errno;
}

static int kqueue_recv(struct kqueue_context* ctx, int flags, int error)
{
	ssize_t r = 0;
	if(0 != error)
	{
		assert(1 == flags); // only in kevent thread
		ctx->in.recv.proc(ctx->in.recv.param, error, 0);
		return error;
	}

	r = recv(ctx->socket[0], ctx->in.recv.buffer, ctx->in.recv.bytes, 0);
	if(r >= 0)
	{
		ctx->in.recv.proc(ctx->in.recv.param, 0, (size_t)r);
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		ctx->in.recv.proc(ctx->in.recv.param, errno, 0);
		return 0;
	}
}

int aio_socket_recv(aio_socket_t socket, void* buffer, size_t bytes, aio_onrecv proc, void* param)
{
    int r = -1;
    struct kqueue_context* ctx = (struct kqueue_context*)socket;
    assert(0 == ctx->ev[0].filter);
    if(ctx->ev[0].filter)
        return EBUSY;

	ctx->in.recv.proc = proc;
	ctx->in.recv.param = param;
	ctx->in.recv.buffer = buffer;
	ctx->in.recv.bytes = bytes;

//    r = kqueue_recv(ctx, 0, 0);
//	if(EAGAIN == r)
	{
		KQueueRead(ctx, kqueue_recv);
	}

    return 0 == r ? 0 : errno;
}

static int kqueue_send(struct kqueue_context* ctx, int flags, int error)
{
	ssize_t r = 0;
	if(0 != error)
	{
		assert(1 == flags); // only in kevent thread
		ctx->out.send.proc(ctx->out.send.param, error, 0);
		return error;
	}

	r = send(ctx->socket[1], ctx->out.send.buffer, ctx->out.send.bytes, 0);
	if(r >= 0)
	{
		ctx->out.send.proc(ctx->out.send.param, 0, (size_t)r);
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		ctx->out.send.proc(ctx->out.send.param, errno, 0);
		return 0;
	}
}

int aio_socket_send(aio_socket_t socket, const void* buffer, size_t bytes, aio_onsend proc, void* param)
{
    int r = -1;
    struct kqueue_context* ctx = (struct kqueue_context*)socket;
    assert(0 == ctx->ev[1].filter);
    if(ctx->ev[1].filter)
        return EBUSY;

	ctx->out.send.proc = proc;
	ctx->out.send.param = param;
	ctx->out.send.buffer = buffer;
	ctx->out.send.bytes = bytes;

//    r = kqueue_send(ctx, 0, 0);
//	if(EAGAIN == r)
	{
        KQueueWrite(ctx, kqueue_send);
	}

    return 0 == r ? 0 : errno;
}

static int kqueue_recv_v(struct kqueue_context* ctx, int flags, int error)
{
	ssize_t r = 0;
	struct msghdr msg;

	if(0 != error)
	{
		assert(1 == flags); // only in kevent thread
		ctx->in.recv_v.proc(ctx->in.recv_v.param, error, 0);
		return error;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = (struct iovec*)ctx->in.recv_v.vec;
	msg.msg_iovlen = ctx->in.recv_v.n;

	r = recvmsg(ctx->socket[0], &msg, 0);
	if(r >= 0)
	{
		ctx->in.recv_v.proc(ctx->in.recv_v.param, 0, (size_t)r);
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		ctx->in.recv_v.proc(ctx->in.recv_v.param, errno, 0);
		return 0;
	}
}

int aio_socket_recv_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecv proc, void* param)
{
    int r = -1;
    struct kqueue_context* ctx = (struct kqueue_context*)socket;
    assert(0 == ctx->ev[0].filter);
    if(ctx->ev[0].filter)
        return EBUSY;

	ctx->in.recv_v.proc = proc;
	ctx->in.recv_v.param = param;
	ctx->in.recv_v.vec = vec;
	ctx->in.recv_v.n = n;

//    r = kqueue_recv_v(ctx, 0, 0);
//	if(EAGAIN == r)
	{
        KQueueRead(ctx, kqueue_recv_v);
	}

    return 0 == r ? 0 : errno;
}

static int kqueue_send_v(struct kqueue_context* ctx, int flags, int error)
{
	ssize_t r = 0;
	struct msghdr msg;

    if(0 != error)
	{
		assert(1 == flags); // only in kevent thread
		ctx->out.send_v.proc(ctx->out.send_v.param, error, 0);
		return error;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = (struct iovec*)ctx->out.send_v.vec;
	msg.msg_iovlen = ctx->out.send_v.n;

	r = sendmsg(ctx->socket[1], &msg, 0);
	if(r >= 0)
	{
		ctx->out.send_v.proc(ctx->out.send_v.param, 0, (size_t)r);
        return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		ctx->out.send_v.proc(ctx->out.send_v.param, errno, 0);
		return 0;
	}
}

int aio_socket_send_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onsend proc, void* param)
{
    int r = -1;
    struct kqueue_context* ctx = (struct kqueue_context*)socket;
    assert(0 == ctx->ev[1].filter);
    if(ctx->ev[1].filter)
        return EBUSY;

	ctx->out.send_v.proc = proc;
	ctx->out.send_v.param = param;
	ctx->out.send_v.vec = vec;
	ctx->out.send_v.n = n;

//    r = kqueue_send_v(ctx, 0, 0);
//	if(EAGAIN == r)
	{
        KQueueWrite(ctx, kqueue_send_v);
	}

    return 0 == r ? 0 : errno;
}

static int kqueue_recvfrom(struct kqueue_context* ctx, int flags, int error)
{
	ssize_t r = 0;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);

	if(0 != error)
	{
		assert(1 == flags); // only in kevent thread
		ctx->in.recvfrom.proc(ctx->in.recvfrom.proc, error, 0, NULL, 0);
		return error;
	}

	r = recvfrom(ctx->socket[0], ctx->in.recvfrom.buffer, ctx->in.recvfrom.bytes, 0, (struct sockaddr*)&addr, &addrlen);
	if(r >= 0)
	{
		ctx->in.recvfrom.proc(ctx->in.recvfrom.param, 0, (size_t)r, (struct sockaddr*)&addr, addrlen);
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		ctx->in.recvfrom.proc(ctx->in.recvfrom.proc, errno, 0, NULL, 0);
		return 0;
	}
}

int aio_socket_recvfrom(aio_socket_t socket, void* buffer, size_t bytes, aio_onrecvfrom proc, void* param)
{
    int r = -1;
    struct kqueue_context* ctx = (struct kqueue_context*)socket;
    assert(0 == ctx->ev[0].filter);
    if(ctx->ev[0].filter)
        return EBUSY;

	ctx->in.recvfrom.proc = proc;
	ctx->in.recvfrom.param = param;
	ctx->in.recvfrom.buffer = buffer;
	ctx->in.recvfrom.bytes = bytes;

//    r = kqueue_recvfrom(ctx, 0, 0);
//	if(EAGAIN == r)
	{
        KQueueRead(ctx, kqueue_recvfrom);
	}

    return 0 == r ? 0 : errno;
}

static int kqueue_sendto(struct kqueue_context* ctx, int flags, int error)
{
	ssize_t r = 0;
	if(0 != error)
	{
		assert(1 == flags); // only in kevent thread
		ctx->out.send.proc(ctx->out.send.param, error, 0);
		return error;
	}

	r = sendto(ctx->socket[1], ctx->out.send.buffer, ctx->out.send.bytes, 0, (struct sockaddr*)&ctx->out.send.addr, ctx->out.send.addrlen);
	if(r >= 0)
	{
		ctx->out.send.proc(ctx->out.send.param, 0, (size_t)r);
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		ctx->out.send.proc(ctx->out.send.param, errno, 0);
		return 0;
	}
}

int aio_socket_sendto(aio_socket_t socket, const struct sockaddr *addr, socklen_t addrlen, const void* buffer, size_t bytes, aio_onsend proc, void* param)
{
    int r = -1;
    struct kqueue_context* ctx = (struct kqueue_context*)socket;
    assert(0 == ctx->ev[1].filter);
    if(ctx->ev[1].filter)
        return EBUSY;

	ctx->out.send.addrlen = addrlen > sizeof(ctx->out.send.addr) ? sizeof(ctx->out.send.addr) : addrlen;
	memcpy(&ctx->out.send.addr, addr, ctx->out.send.addrlen);
	ctx->out.send.proc = proc;
	ctx->out.send.param = param;
	ctx->out.send.buffer = buffer;
	ctx->out.send.bytes = bytes;

//    r = kqueue_sendto(ctx, 0, 0);
//	if(EAGAIN == r)
	{
        KQueueWrite(ctx, kqueue_sendto);
	}

    return 0 == r ? 0 : errno;
}

static int kqueue_recvfrom_v(struct kqueue_context* ctx, int flags, int error)
{
	ssize_t r;
	struct msghdr msg;
	struct sockaddr_storage addr;

	if(0 != error)
	{
		assert(1 == flags); // only in kevent thread
		ctx->in.recvfrom_v.proc(ctx->in.recvfrom_v.param, error, 0, NULL, 0);
		return error;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = (struct iovec*)ctx->in.recvfrom_v.vec;
	msg.msg_iovlen = ctx->in.recvfrom_v.n;

	r = recvmsg(ctx->socket[0], &msg, 0);
	if(r >= 0)
	{
		ctx->in.recvfrom_v.proc(ctx->in.recvfrom_v.param, 0, (size_t)r, (struct sockaddr*)&addr, msg.msg_namelen);
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		ctx->in.recvfrom_v.proc(ctx->in.recvfrom_v.param, errno, 0, NULL, 0);
		return 0;
	}
}

int aio_socket_recvfrom_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecvfrom proc, void* param)
{
    int r = -1;
    struct kqueue_context* ctx = (struct kqueue_context*)socket;
    assert(0 == ctx->ev[0].filter);
    if(ctx->ev[0].filter)
        return EBUSY;

	ctx->in.recvfrom_v.proc = proc;
	ctx->in.recvfrom_v.param = param;
	ctx->in.recvfrom_v.vec = vec;
	ctx->in.recvfrom_v.n = n;

//    r = kqueue_recvfrom_v(ctx, 0, 0);
//	if(EAGAIN == r)
	{
        KQueueRead(ctx, kqueue_recvfrom_v);
	}

    return 0 == r ? 0 : errno;
}

static int kqueue_sendto_v(struct kqueue_context* ctx, int flags, int error)
{
	ssize_t r = 0;
	struct msghdr msg;

	if(0 != error)
	{
		assert(1 == flags); // only in kevent thread
		ctx->out.send_v.proc(ctx->out.send_v.param, error, 0);
		return error;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (struct sockaddr*)&ctx->out.send_v.addr;
	msg.msg_namelen = ctx->out.send_v.addrlen;
	msg.msg_iov = (struct iovec*)ctx->out.send_v.vec;
	msg.msg_iovlen = ctx->out.send_v.n;

	r = sendmsg(ctx->socket[1], &msg, 0);
	if(r >= 0)
	{
		ctx->out.send_v.proc(ctx->out.send_v.param, 0, (size_t)r);
		return 0;
	}
	else
	{
		if(0 == flags)
			return errno;

		ctx->out.send_v.proc(ctx->out.send_v.param, errno, 0);
		return 0;
	}
}

int aio_socket_sendto_v(aio_socket_t socket, const struct sockaddr *addr, socklen_t addrlen, socket_bufvec_t* vec, int n, aio_onsend proc, void* param)
{
    int r = -1;
	struct kqueue_context* ctx = (struct kqueue_context*)socket;
	assert(0 == ctx->ev[1].filter);
	if(ctx->ev[1].filter)
		return EBUSY;

	ctx->out.send_v.addrlen = addrlen > sizeof(ctx->out.send_v.addr) ? sizeof(ctx->out.send_v.addr) : addrlen;
	memcpy(&ctx->out.send_v.addr, addr, ctx->out.send_v.addrlen);
	ctx->out.send_v.proc = proc;
	ctx->out.send_v.param = param;
	ctx->out.send_v.vec = vec;
	ctx->out.send_v.n = n;

//    r = kqueue_sendto_v(ctx, 0, 0);
//	if(EAGAIN == r)
	{
        KQueueWrite(ctx, kqueue_sendto_v);
	}

    return 0 == r ? 0 : errno;
}

#endif