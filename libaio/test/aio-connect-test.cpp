#include "aio-connect.h"
#include "aio-timeout.h"
#include "aio-socket.h"
#include "sys/system.h"
#include <stdio.h>

static void aio_connect_baidu(void* param, int code, socket_t tcp, aio_socket_t aio)
{
	uint64_t now = system_clock();
	uint64_t* clock = (uint64_t*)param;
	printf("connect baidu: %d, time: %d\n", code, (int)(now - *clock));
	aio_socket_destroy(aio, NULL, NULL);
}

static void aio_connect_google(void* param, int code, socket_t tcp, aio_socket_t aio)
{
	uint64_t now = system_clock();
	uint64_t* clock = (uint64_t*)param;
	printf("connect google: %d, time: %d\n", code, (int)(now - *clock));
	aio_socket_destroy(aio, NULL, NULL);
}

extern "C" void aio_connect_test()
{
	aio_socket_init(1);

	uint64_t clock = system_clock();
	aio_connect("www.baidu.com", 80, 3000, aio_connect_baidu, &clock);
	aio_connect("www.google.com", 80, 3000, aio_connect_google, &clock);

	for(int i = 0; i < 10; i++)
	{
		aio_socket_process(1000);
		aio_timeout_process();
	}

	aio_socket_clean();
}
