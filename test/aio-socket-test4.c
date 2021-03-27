#include "cstringext.h"
#include "aio-socket.h"
#include "sys/system.h"
#include "sys/thread.h"
#include "sockutil.h"
#include <errno.h>

static char msg[10/*24*1024*8*/];

static int STDCALL worker0(IN void* param)
{
	socket_t socket;
	socket_t client;
	struct sockaddr_storage addr;
	socklen_t len = sizeof(addr);

	param = param;
	socket = socket_tcp_listen_ipv4(NULL, 8008, 64);

//	while(1)
	{
		client = socket_accept(socket, &addr, &len);
		//socket_recv(client, msg, sizeof(msg), 0);
		//socket_recv(client, msg, 10, 0);
		//system_sleep(1000);
		//socket_close(client);
	}

	socket_close(socket);
	return 0;
}

static int STDCALL worker1(IN void* aiosocket)
{
	while(1)
	{
		int r;
		r = aio_socket_process(1000);
		printf("aio_socket_process => %d/%d\n", r, errno);
	}

//	aio_socket_destroy(aiosocket);
	return 0;
}

static void OnSend(void* aiosocket, int code, size_t bytes)
{
	printf("onsend: %u, code: %d, errno: %d\n", (unsigned int)bytes, code, errno);

	system_sleep(2000);
	if(0 == code)
		aio_socket_send(aiosocket, msg, sizeof(msg), OnSend, aiosocket);
}

static void OnDestroy(void* param)
{
}

void aio_socket_test4(void)
{
	socket_t socket;
	aio_socket_t aiosocket;
	pthread_t thread[3];

	socket_init();
	aio_socket_init(1);
	thread_create(&thread[0], worker0, NULL);
	thread_create(&thread[1], worker1, NULL);
	thread_create(&thread[2], worker1, NULL);

	system_sleep(3000);

	printf("aio tcp socket\n");
	socket = socket_connect_host("127.0.0.1", 8008, 5000);
	printf("connect:%d\n", errno);
	aiosocket = aio_socket_create(socket, 1);
	//system_sleep(5000);
	aio_socket_send(aiosocket, msg, sizeof(msg), OnSend, aiosocket);
	//system_sleep(5000);
	aio_socket_destroy(aiosocket, OnDestroy, NULL);

	//printf("aio udp socket\n");
	//socket = socket_udp();
	//r = socket_bind_any(socket, (unsigned short)8089);
	//printf("socket bind: %d\n", r);
	//aiosocket = aio_socket_create(socket, 1);
	//aio_socket_destroy(aiosocket);

	thread_destroy(thread[0]);
	thread_destroy(thread[1]);
	thread_destroy(thread[2]);
	aio_socket_clean();
	socket_cleanup();
}
//
//int main(int argc, char* argv[])
//{
//	aio_socket_test4();
//	return 0;
//}
