#include "sys-thread-pool.h"
#include "thread-pool.h"

extern thread_pool_t g_pool;

int sys_thread_pool_push(sys_thread_pool_proc proc, void *param)
{
	return thread_pool_push(g_pool, proc, param);
}
