#include "aio-socket.h"
#include "aio-timeout.h"
#include "http-server.h"
#include "http-route.h"
#include "sys/thread.h"
#include "sys/system.h"

static int STDCALL http_server_worker(void* param)
{
	int idx = (int)(intptr_t)param;
	while (aio_socket_process(200) >= 0 || errno == EINTR) // ignore epoll EINTR
	{
	}

	printf("%s[%d] exit => %d.\n", __FUNCTION__, idx, errno);
	return 0;
}

extern "C" void http_server_test(const char* ip, int port)
{
	size_t num = 0;

//	num = system_getcpucount() * 2;
	aio_socket_init(num + 1);

	http_server_t* http = http_server_create(ip, port);
	http_server_set_handler(http, http_server_route, http);

	// aio worker
	while (num-- > 0)
	{
		pthread_t thread;
		thread_create(&thread, http_server_worker, (void*)(intptr_t)num);
		thread_detach(thread);
	}

	// timeout process
	while (aio_socket_process(10000) >= 0)
	{
		aio_timeout_process();
	}

	http_server_destroy(http);
	aio_socket_clean();
}
