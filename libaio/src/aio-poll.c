#include "aio-poll.h"
#include "sys/system.h"
#include "sys/thread.h"
#include "sys/locker.h"
#include "sys/pollfd.h"
#include "port/socketpair.h"
#include "sockutil.h"
#include "list.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

#define N_SOCKETS (FD_SETSIZE-1)

struct aio_poll_socket_t
{
	struct list_head link;

	socket_t fd;
	int events;
	int revents;
	uint64_t expire;
	aio_poll_onpoll callback;
	void* param;
};

struct aio_poll_t
{
	locker_t locker;
	socket_t pair[2];
	struct list_head root;
	struct list_head idles;

	int running;
	pthread_t thread;
};

static int STDCALL aio_poll_worker(void* param);

static void aio_poll_init_idles(struct aio_poll_t* poll)
{
	int i;
	struct aio_poll_socket_t* s;
	for (i = 0; i < N_SOCKETS; i++)
	{
		s = (struct aio_poll_socket_t*)(poll + 1) + i;
		list_insert_after(&s->link, &poll->idles);
	}
}

static struct aio_poll_socket_t* aio_poll_alloc(struct aio_poll_t* poll)
{
	struct list_head* item;
	locker_lock(&poll->locker);
	if (list_empty(&poll->idles))
	{
		locker_unlock(&poll->locker);
		return NULL;
	}

	item = poll->idles.next;
	list_remove(item);
	locker_unlock(&poll->locker);
	return list_entry(item, struct aio_poll_socket_t, link);
}

static void aio_poll_free(struct aio_poll_t* poll, struct aio_poll_socket_t* s)
{
	// TODO: assert(idle count < N);
	locker_lock(&poll->locker);
	list_remove(&s->link);
	list_insert_after(&s->link, &poll->idles);
	locker_unlock(&poll->locker);
}

struct aio_poll_t* aio_poll_create(void)
{
	struct aio_poll_t* poll;
	poll = (struct aio_poll_t*)calloc(1, sizeof(struct aio_poll_t) + sizeof(struct aio_poll_socket_t) * N_SOCKETS);
	if (poll)
	{
		LIST_INIT_HEAD(&poll->root);
		LIST_INIT_HEAD(&poll->idles);
		aio_poll_init_idles(poll);

#if defined(OS_WINDOWS)
		if (0 != socketpair(PF_INET, SOCK_DGRAM, 0, poll->pair))
#else
        if (0 != socketpair(AF_UNIX, SOCK_STREAM, 0, poll->pair))
#endif
		{
			free(poll);
			return NULL;
		}

		poll->running = 1;
		locker_create(&poll->locker);
		thread_create(&poll->thread, aio_poll_worker, poll);
	}
	return poll;
}

int aio_poll_destroy(struct aio_poll_t* poll)
{
	if (!poll || 0 == poll->running)
		return -1;

	assert(list_empty(&poll->root));
	poll->running = 0;
	socket_send_all_by_time(poll->pair[1], poll, 1, 0, 5000);

	thread_destroy(poll->thread);
	locker_destroy(&poll->locker);
	socket_close(poll->pair[0]);
	socket_close(poll->pair[1]);
	return 0;
}

int aio_poll_poll(struct aio_poll_t* poll, socket_t socket, int flags, int timeout, aio_poll_onpoll callback, void* param)
{
	struct aio_poll_socket_t* s;

	if (0 == (flags & (AIO_POLL_IN | AIO_POLL_OUT)) || !callback || timeout <= 0 || socket_invalid == socket)
		return EINVAL;

	s = aio_poll_alloc(poll);
	if (!s)
		return ENOMEM;

	memset(s, 0, sizeof(*s));
	s->fd = socket;
	s->events = flags;
	s->expire = system_clock() + timeout;
	s->callback = callback;
	s->param = param;

	locker_lock(&poll->locker);
	list_insert_after(&s->link, &poll->root);
	locker_unlock(&poll->locker);

	// notify
	if(!thread_isself(poll->thread))
		socket_send_all_by_time(poll->pair[1], s, 1, 0, 5000);
	return 0;
}

static void aio_poll_doerror(struct aio_poll_t* poll, struct aio_poll_socket_t* s[], int n);
static int aio_poll_do(struct aio_poll_socket_t* s[], int n, int timeout);
static int STDCALL aio_poll_worker(void* param)
{
	int i, n, r;
	uint64_t now;
	char buf[128];
	struct list_head* ptr;
	struct aio_poll_t* poll;
	struct aio_poll_socket_t link0, *links[N_SOCKETS];

	poll = (struct aio_poll_t*)param;
	link0.fd = poll->pair[0];
	link0.events = AIO_POLL_IN;
	
	while (poll->running)
	{
		socket_recv_by_time(poll->pair[0], buf, sizeof(buf), 0, 0);

		n = 0;
		links[n++] = &link0;
		locker_lock(&poll->locker);
		list_for_each(ptr, &poll->root)
		{
			links[n++] = list_entry(ptr, struct aio_poll_socket_t, link);
		}
		locker_unlock(&poll->locker);

		r = aio_poll_do(links, n, 10*1000);
		if (r < 0)
		{
			//WSAENOTSOCK;
			//r = socket_geterror();
			aio_poll_doerror(poll, links, n);
			continue;
		}

		now = system_clock();
		for (i = 1; i < n; i++)
		{
			if (0 != links[i]->revents)
			{
				links[i]->callback(0, links[i]->fd, links[i]->revents, links[i]->param);
				aio_poll_free(poll, links[i]);
			}
			else if ((int64_t)(now - links[i]->expire) > 0)
			{
				links[i]->callback(ETIMEDOUT, links[i]->fd, 0, links[i]->param);
				aio_poll_free(poll, links[i]);
			}
			else
			{
				// next loop
			}
		}
	}

	return 0;
}

static int aio_poll_do(struct aio_poll_socket_t* s[], int n, int timeout)
{
	int i;
	int r;

#if defined(OS_WINDOWS)
	fd_set rfds;
	fd_set wfds;
	fd_set efds;
	struct timeval tv;

	assert(n <= FD_SETSIZE);
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);
	for (i = 0; i < n; i++)
	{
		if (s[i]->events & AIO_POLL_IN)
			FD_SET(s[i]->fd, &rfds);
		if (s[i]->events & AIO_POLL_OUT)
			FD_SET(s[i]->fd, &wfds);
	}

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;
	r = select(n, &rfds, &wfds, &efds, timeout < 0 ? NULL : &tv);
	if (r <= 0)
		return r;

	for (r = i = 0; i < n && i < 64; i++)
	{
		s[i]->revents = 0;
		if (FD_ISSET(s[i]->fd, &rfds) && (AIO_POLL_IN & s[i]->events))
			s[i]->revents |= AIO_POLL_IN;
		if (FD_ISSET(s[i]->fd, &wfds) && (AIO_POLL_OUT & s[i]->events))
			s[i]->revents |= AIO_POLL_OUT;
	}

	return r;
#else
	struct pollfd fds[N_SOCKETS];
	assert(n < N_SOCKETS);
	for (i = 0; i < n; i++)
	{
		fds[i].fd = s[i]->fd;
		fds[i].revents = 0;
		if (s[i]->events & AIO_POLL_IN)
			fds[i].events = POLLIN;
		if (s[i]->events & AIO_POLL_OUT)
			fds[i].events = POLLOUT;
	}

	r = poll(fds, n, timeout);
	while (-1 == r && (EINTR == errno || EAGAIN == errno))
		r = poll(fds, n, timeout);

	for (r = i = 0; i < n && i < 64; i++)
	{
		s[i]->revents = 0;
		if (fds[i].revents & POLLIN)
			s[i]->revents |= AIO_POLL_IN;
		if (fds[i].revents & POLLOUT)
			s[i]->revents |= AIO_POLL_OUT;
	}

	return r;
#endif
}

static void aio_poll_doerror(struct aio_poll_t* poll, struct aio_poll_socket_t* s[], int n)
{
	int i, r;
	for (i = 1; i < n; i++)
	{
		if (s[i]->events & AIO_POLL_IN)
			r = socket_select_read(s[i]->fd, 0);
		else if (s[i]->events & AIO_POLL_OUT)
			r = socket_select_write(s[i]->fd, 0);
		else
			r = -1;

		if(r < 0)
		{
			s[i]->callback(EINVAL, s[i]->fd, 0, s[i]->param);
			aio_poll_free(poll, s[i]);
		}
	}
}
