#if defined(_DEBUG) || defined(DEBUG)
#include "cstringext.h"
#include "sys/sock.h"
#include "sys/system.h"
#include "sys/thread.h"
#include "aio-socket.h"
#include "http-client.h"

#define MAX_THREAD 8

static int s_running = 0;
static pthread_t s_threads[MAX_THREAD];

static int STDCALL aio_worker_action(void* param)
{
	do
	{
		aio_socket_process(30*1000);
	} while(*(int*)param);

	return 0;
}

static int aio_worker_init(void)
{
	size_t i;
	s_running = 1;
	aio_socket_init(MAX_THREAD);
	for(i = 0; i < MAX_THREAD; i++)
	{
		if(0 != thread_create(&s_threads[i], aio_worker_action, &s_running))
		{
			exit(-1);
		}
	}
	return 0;
}

static int aio_worker_cleanup(void)
{
	size_t i;
	s_running = 0;
	for(i = 0; i < MAX_THREAD; i++)
		thread_destroy(s_threads[i]);
	aio_socket_clean();
	return 0;
}

static void http_client_test_onreply(void*, void *http, int code)
{
	if(0 == code)
	{
		const char* server = http_client_get_header(http, "Server");
		if(server)
			printf("http server: %s\n", server);
	}
	else
	{
		printf("http server reply error: %d\n", code);
	}
}

void http_client_test(void)
{
	aio_worker_init();

	int r;
	struct http_header_t headers[3];
	headers[0].name = "User-Agent";
	headers[0].value = "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:34.0) Gecko/20100101 Firefox/34.0";
	headers[1].name = "Accept-Language";
	headers[1].value = "en-US,en;q=0.5";
	headers[2].name = "Connection";
	headers[2].value = "keep-alive";

	void *http = http_client_create("www.baidu.com", 80, 0);
	r = http_client_get(http, "/", headers, sizeof(headers)/sizeof(headers[0]), http_client_test_onreply, http);
	r = http_client_get(http, "/img/bdlogo.png", headers, sizeof(headers)/sizeof(headers[0]), http_client_test_onreply, http);
	r = http_client_get(http, "/", headers, sizeof(headers)/sizeof(headers[0]), http_client_test_onreply, http);
	r = http_client_get(http, "/", headers, sizeof(headers)/sizeof(headers[0]), http_client_test_onreply, http);
	system_sleep(10000);
	http_client_destroy(http);

	aio_worker_cleanup();
}

#endif
