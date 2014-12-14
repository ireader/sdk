#include "thread-pool.h"
#include "cstringext.h"
#include "sys/system.h"
#include "sys/atomic.h"
#include <stdlib.h>
#include <assert.h>
#include <time.h>

static int32_t total = 0;

static void worker(void* param)
{
	int i = 0;
	int n = *(int*)param + 1;

	while(i++ < n)
	{
#if defined(OS_MAC)
        system_sleep(arc4random() % 30);
#else
		system_sleep(rand() % 30);
#endif
		if(210 == atomic_increment32(&total))
		{
			printf("[%d] I'm the KING\n", n);
			assert(i == n);
			break;
		}
	}

	printf("[%d] done\n", n);
}

void thread_pool_test(void)
{
	int i, r;
	thread_pool_t pool;
	pool = thread_pool_create(4, 2, 8);

	for(i=0; i<20; i++)
	{
		r = thread_pool_push(pool, worker, &i);
		assert(0 == r);
	}
	
	thread_pool_destroy(pool);
}
