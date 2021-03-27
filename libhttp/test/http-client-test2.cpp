#if defined(_DEBUG) || defined(DEBUG)
// 测试自动重连功能

#include "cstringext.h"
#include "sockutil.h"
#include "sys/atomic.h"
#include "sys/system.h"
#include "sys/thread.h"
#include "http-client.h"
#include "http-transport.h"
#include "aio-socket.h"
#include <assert.h>

#define PORT 1234

static const char* s_reply = "HTTP/1.1 200 OK\r\nServer: Test2\r\nContent-Length: 0\r\n\r\n";

#define MAX_THREAD 8

static int s_running = 0;
static pthread_t s_threads[MAX_THREAD];

static int STDCALL aio_worker_action(void* param)
{
	do
	{
		aio_socket_process(1000);
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

static int STDCALL http_server_thread(void* param)
{
	char req[1024] = {0};
	bool* running = (bool*)param;
	socket_t socket = socket_tcp_listen_ipv4(NULL, PORT, SOMAXCONN);

	while(*running)
	{
		int r = socket_select_read(socket, 1000);
		if(1 == r)
		{
			struct sockaddr_storage ss;
			socklen_t len = sizeof(ss);
			socket_t client = socket_accept(socket, &ss, &len);
			if(client != socket_invalid)
			{
				r = socket_recv_by_time(client, req, sizeof(req), 0, 5000);
				r = socket_send_all_by_time(client, s_reply, strlen(s_reply), 0, 5000);
				r = socket_shutdown(client, SHUT_RDWR);
				r = socket_close(client);
				//printf("server side close socket\n");
			}
		}
	}

	return 0;
}

static void http_client_test_onreply(void* param, int code, int http_status_code, int64_t http_content_length)
{
	http_client_t* http = (http_client_t*)param;
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

static void http_client_test_onreply2(void* param, int code, int http_status_code, int64_t http_content_length)
{
	static int i = 0;
	http_client_t* http = (http_client_t*)param;

	if (i++ < 1000)
	{
		assert(0 == http_client_get(http, "/", NULL, 0, http_client_test_onreply2, http));
	}
	else
	{
		http_client_destroy(http);
	}
}

extern "C" void http_client_test2(void)
{
	aio_worker_init();

	pthread_t thread;
	bool running = true;
	thread_create(&thread, http_server_thread, &running);
	system_sleep(1000); // switch to http_server_thread

	struct http_header_t headers[3];
	headers[0].name = "User-Agent";
	headers[0].value = "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:34.0) Gecko/20100101 Firefox/34.0";
	headers[1].name = "Accept-Language";
	headers[1].value = "en-US,en;q=0.5";
	headers[2].name = "Connection";
	headers[2].value = "keep-alive";

	// block IO
	http_client_t *http = http_client_create(http_transport_default(), "http", "127.0.0.1", PORT);
	assert(0 == http_client_get(http, "/", headers, sizeof(headers)/sizeof(headers[0]), http_client_test_onreply, http));
	assert(0 == http_client_get(http, "/img/bdlogo.png", headers, sizeof(headers)/sizeof(headers[0]), http_client_test_onreply, http));
	assert(0 == http_client_get(http, "/", NULL, 0, http_client_test_onreply, http));
	http_client_destroy(http);

	// AIO
	http = http_client_create(http_transport_default_aio(), "http", "127.0.0.1", PORT);
	assert(0 == http_client_get(http, "/", headers, sizeof(headers)/sizeof(headers[0]), http_client_test_onreply2, http));
	system_sleep(10000);

	running = false;
	thread_destroy(thread);
	aio_worker_cleanup();

	printf("http client NIO test ok\n");
}

#endif
