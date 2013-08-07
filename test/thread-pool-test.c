#include "thread-pool.h"
#include "cstringext.h"
#include "sys/system.h"
#include <stdlib.h>
#include <assert.h>
#include <time.h>

static long total = 0;

void worker(void* param)
{
	int i = 0;
	int n = (int)param;

	srand(time(NULL));
	while(i++ < n)
	{
		system_sleep(rand() % 30);
		if(210 == InterlockedIncrement(&total))
		{
			printf("[%d] I'm the KING\n", n);
			assert(i == n);
			break;
		}
	}

	printf("[%d] done\n", n);
}

void thread_pool_test()
{
	int i, r;
	thread_pool_t pool;
	pool = thread_pool_create(4, 2, 8);

	for(i=0; i<20; i++)
	{
		r = thread_pool_push(pool, worker, (void*)(i+1));
		assert(0 == r);
	}
	
	thread_pool_destroy(pool);
}
