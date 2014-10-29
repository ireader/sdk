#include "cstringext.h"
#include "sys/locker.h"
#include "sys/thread.h"
#include "sys/system.h"

#define N_THREADS 100

static unsigned char value[N_THREADS];
static locker_t locker;

static int STDCALL worker(void* param)
{
	int i, j;
	unsigned char v[N_THREADS];
	int p = *(int*)param;

	for(i = 0; i < 10000; i++)
	{
		locker_lock(&locker);
		memset(value, p, sizeof(value));
		memcpy(v, value, sizeof(v));
		locker_unlock(&locker);
		for(j = 0; j < sizeof(v); j++)
		{
			assert(v[j] == (unsigned char)p);
		}
	}

	return 0;
}

void locker_test(void)
{
	int i;
	pthread_t threads[N_THREADS];

	locker_create(&locker);
	for(i = 0; i < N_THREADS; i++)
	{
		thread_create(&threads[i], worker, &i);
	}

	for(i = 0; i < N_THREADS; i++)
	{
		thread_destroy(threads[i]);
	}

	locker_destroy(&locker);
}
