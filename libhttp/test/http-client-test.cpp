#if defined(_DEBUG) || defined(DEBUG)
#include "sys/sock.h"
#include "http-client.h"

static void http_client_test_onreply(void* param, int code)
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

extern "C" void http_client_test(void)
{
	struct http_header_t headers[3];
	headers[0].name = "User-Agent";
	headers[0].value = "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:34.0) Gecko/20100101 Firefox/34.0";
	headers[1].name = "Accept-Language";
	headers[1].value = "en-US,en;q=0.5";
	headers[2].name = "Connection";
	headers[2].value = "keep-alive";

	socket_init();
	http_client_t *http = http_client_create("www.baidu.com", 80, 1);
	assert(0 == http_client_get(http, "/", headers, sizeof(headers) / sizeof(headers[0]), http_client_test_onreply, http));
	assert(0 == http_client_get(http, "/img/bdlogo.png", headers, sizeof(headers)/sizeof(headers[0]), http_client_test_onreply, http));
	assert(0 == http_client_get(http, "/", headers, sizeof(headers)/sizeof(headers[0]), http_client_test_onreply, http));
	assert(0 == http_client_get(http, "/", headers, sizeof(headers)/sizeof(headers[0]), http_client_test_onreply, http));
	http_client_destroy(http);
	socket_cleanup();
}

#endif
