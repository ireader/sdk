#if defined(_DEBUG) || defined(DEBUG)
#include "cstringext.h"
#include "sys/sock.h"
#include "sys/system.h"
#include "sys/process.h"
#include "aio-socket.h"
#include "thread-pool.h"
#include "http-server.h"

static int s_running;
static thread_pool_t s_pool;

static void worker(void* param)
{
	int r;
	do
	{
		r = aio_socket_process(2*60*1000);
		if(0 != r)
		{
			//printf("http_server_process =>%d\n", r);
		}
	} while(*(int*)param && -1 != r);
}

static int init()
{
	int cpu = (int)system_getcpucount();
	s_pool = thread_pool_create(cpu, 1, 64);
	aio_socket_init(cpu);

	s_running = 1;
	while(cpu-- > 0)
	{
		thread_pool_push(s_pool, worker, &s_running); // start worker
	}
	
	return 0;
}

static int cleanup()
{
	s_running = 0;
	thread_pool_destroy(s_pool);
	aio_socket_clean();
	return 0;
}

static int handler(void* param, void* session, const char* method, const char* path)
{
	return http_server_send(session, 200, param);
}

void http_benchmark()
{
	void *http;
	void *bundle;
	void *ptr;

	init();

	bundle = http_bundle_alloc(1024);
	ptr = http_bundle_lock(bundle);
	strcpy((char*)ptr, "Hello World!");
	http_bundle_unlock(bundle, strlen((char*)ptr));

	http = http_server_create(NULL, 8888);
	http_server_set_handler(http, handler, bundle);

	while(1)
	{
		system_sleep(10000);
	}
}

#endif
