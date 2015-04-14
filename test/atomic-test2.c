#include "cstringext.h"
#include "sys/atomic.h"
#include "sys/thread.h"
#include "sys/system.h"
#include <assert.h>

#define N_THREAD 32

static int32_t s_v32 = 100;
static int64_t s_v64 = 0x0011001100110011;

static int STDCALL atomic_thread(void* param)
{
	int i;
	(void)param;

	for(i = 0; i < 10000000; i++)
	{
		atomic_increment32(&s_v32);
		atomic_increment64(&s_v64);
		atomic_decrement32(&s_v32);
		atomic_decrement64(&s_v64);
	}
	return 0;
}

void atomic_test2(void)
{
	int i = 0;
	pthread_t threads[N_THREAD];

	for(i = 0; i < N_THREAD; i++)
	{
		thread_create(&threads[i], atomic_thread, NULL);
	}

	for(i = 0; i < N_THREAD; i++)
	{
		thread_destroy(threads[i]);
	}

	assert(s_v32 == 100);
	assert(s_v64 == 0x0011001100110011);
}
