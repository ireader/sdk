#include "aio-worker.h"
#include "aio-socket.h"
#include "aio-timeout.h"
#include "sys/thread.h"
#include <stdio.h>
#include <errno.h>

#define VMIN(a, b)	((a) < (b) ? (a) : (b))

static int s_running;
static pthread_t s_thread[1000];

static int STDCALL aio_worker(void* param)
{
	int i = 0, r = 0;
	int idx = (int)(intptr_t)param;
	while (s_running && (r >= 0 || EINTR == errno || EAGAIN == errno)) // ignore epoll EINTR
	{
		r = aio_socket_process(idx ? 2000 : 64);
		if (0 == idx && (0 == r || i++ > 100))
		{
			i = 0;
			aio_timeout_process();
		}
	}

	printf("%s[%d] exit => %d.\n", __FUNCTION__, idx, errno);
	return 0;
}

void aio_worker_init(int num)
{
	s_running = 1;
	num = VMIN(num, sizeof(s_thread) / sizeof(s_thread[0]));
	aio_socket_init(num);

	while (num-- > 0)
	{
		thread_create(&s_thread[num], aio_worker, (void*)(intptr_t)num);
	}
}

void aio_worker_clean(int num)
{
	s_running = 0;
	num = VMIN(num, sizeof(s_thread) / sizeof(s_thread[0]));
	while (num-- > 0)
	{
		thread_destroy(s_thread[num]);
	}

	aio_socket_clean();
}
