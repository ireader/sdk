#include "sys/sock.h"
#include "http-client.h"
#include "http-transport.h"

static void http_client_test_onbody(void* param, int code, void* msg, size_t len)
{
}

static void http_client_test_onreply(void* param, int code, int http_status_code, int64_t http_content_length)
{
	static char buf[2 * 1024 * 1024];
	http_client_t* http = (http_client_t*)param;
	if (0 == code)
	{
		const char* server = http_client_get_header(http, "Server");
		if (server)
			printf("http server: %s\n", server);

		http_client_read(http, buf, sizeof(buf), HTTP_READ_FLAGS_WHOLE, http_client_test_onbody, &http_content_length);
	}
	else
	{
		printf("http server reply error: %d\n", code);
	}
}

static int http_client_test_poll(void*, void* c, uintptr_t fd, int event, int timeout, void (*onevent)(void* c, int event))
{
	int r;

	if (HTTP_TRANSPORT_POLL_READ == event)
	{
		r = socket_select_read((socket_t)fd, timeout);
	}
	else if (HTTP_TRANSPORT_POLL_WRITE == event)
	{
		r = socket_select_write((socket_t)fd, timeout);
	}
	else
	{
		assert(0);
	}

	if (r <= 0)
		return 0 == r ? -ETIMEDOUT : r;

	assert(1 == r);
	onevent(c, event);
	return 0;
}

extern "C" void http_client_test3(void)
{
	socket_init();

	struct http_header_t headers[3];
	headers[0].name = "User-Agent";
	headers[0].value = "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:34.0) Gecko/20100101 Firefox/34.0";
	headers[1].name = "Accept-Language";
	headers[1].value = "en-US,en;q=0.5";
	headers[2].name = "Connection";
	headers[2].value = "keep-alive";

	http_transport_t* t = http_transport_user_poll(http_client_test_poll, NULL);
	http_client_t* http = http_client_create(t, "https", "bing.com", 443);
	assert(0 == http_client_get(http, "/", headers, sizeof(headers) / sizeof(headers[0]), http_client_test_onreply, http));
	assert(0 == http_client_get(http, "/img/bdlogo.png", headers, sizeof(headers) / sizeof(headers[0]), http_client_test_onreply, http));
	assert(0 == http_client_get(http, "/", headers, sizeof(headers) / sizeof(headers[0]), http_client_test_onreply, http));
	assert(0 == http_client_get(http, "/", headers, sizeof(headers) / sizeof(headers[0]), http_client_test_onreply, http));
	http_client_destroy(http);
	http_transport_release(t);
	socket_cleanup();
}
