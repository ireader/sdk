#include "thread-pool.h"
#include "sys/sock.h"
#include "sys/system.h"
#include "systimer.h"

static thread_pool_t sys_init()
{
	thread_pool_t pool;
	size_t num = system_getcpucount();
	pool = thread_pool_create(num*2, num, num*128);
	
	// init timer
	systimer_init(pool);

	// init socket
	socket_init();
	return pool;
}

thread_pool_t g_pool = sys_init();
