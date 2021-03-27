#include "cstringext.h"
#include "aio-socket.h"
#include "sys/system.h"
#include "sys/thread.h"
#include "sockutil.h"
#include <errno.h>

#define PORT 8888

#if defined(OS_WINDOWS) && !defined(EINPROGRESS)
#define EINPROGRESS             WSAEINPROGRESS
#endif

static char msg2[200*1024*1024];

static int STDCALL worker(IN void* param)
{
	socket_t socket;
	socket_t client;
	struct sockaddr_storage addr;
	socklen_t len = sizeof(addr);

	param = param;
	socket = socket_tcp_listen_ipv4(NULL, PORT, 64);

	client = socket_accept(socket, &addr, &len); // recv
	client = socket_accept(socket, &addr, &len); // send

//	socket_close(client);
	socket_close(socket);
	return 0;
}

static void onrecv(void* param, int code, size_t bytes)
{
	//printf("aio_socket_recv_test onrecv: code=%d, bytes=%d\n", code, bytes);
}

static void ondestroy(void* param)
{
	printf("aio socket destroy\n");
}

static int aio_socket_recv_test(void)
{
	int r;
	socket_t socket;
	aio_socket_t aiosocket;
	char msg[1024];
	size_t timeout;

	socket = socket_connect_host("127.0.0.1", PORT, 2000);
	assert(socket_invalid != socket);

	r = socket_getrecvtimeout(socket, &timeout);
	//printf("socket_getrecvtimeout: %d, timeout=%u\n", r, timeout);

	//timeout = 2;
	//r = socket_setrecvtimeout(socket, timeout);
	//printf("socket_setrecvtimeout: %d\n", r);

	aio_socket_init(1);
	aiosocket = aio_socket_create(socket, 0);
	assert(0 == aio_socket_recv(aiosocket, msg, sizeof(msg), onrecv, NULL));

	//////////////////////////////////////////////////////////////////////////
	/// test 1
	timeout = 1;
	assert(0 == socket_setrecvtimeout(socket, timeout));
	r = aio_socket_process(2000);
	printf("recv[1] socket set receive timeout [%d] => %s\n", r, 0==r?"failed" : "OK");
	if(1 == r) aio_socket_recv(aiosocket, msg, sizeof(msg), onrecv, NULL);

	//////////////////////////////////////////////////////////////////////////
	/// test 2
	assert(0 == socket_shutdown(socket, SHUT_WR));
	r = aio_socket_process(2000);
	printf("recv[2] socket shutdown write [%d] => %s\n", r, 0==r?"failed" : "OK");
	if (1 == r) aio_socket_recv(aiosocket, msg, sizeof(msg), onrecv, NULL);

	//////////////////////////////////////////////////////////////////////////
	/// test 3
	assert(0 == socket_shutdown(socket, SHUT_RD));
	r = aio_socket_process(2000);
	printf("recv[3] socket shutdown read [%d] => %s\n", r, 0==r?"failed" : "OK");
	if (1 == r) aio_socket_recv(aiosocket, msg, sizeof(msg), onrecv, NULL);

	//////////////////////////////////////////////////////////////////////////
	/// test 4
	assert(0 == socket_close(socket));
	r = aio_socket_process(2000);
	printf("recv[4] socket close [%d] => %s\n", r, 0==r?"failed" : "OK");

	aio_socket_destroy(aiosocket, ondestroy, NULL);
	aio_socket_process(2000);
	aio_socket_clean();
	return 0;
}

static void onsend(void* param, int code, size_t bytes)
{
	param = param;
    code = code;
    bytes = bytes;
	//printf("aio_socket_send_test onsend: code=%d, bytes=%d\n", code, bytes);
}

static int aio_socket_send_test(void)
{
	int r;
	socket_t socket;
	aio_socket_t aiosocket;
	size_t timeout;

	socket = socket_connect_host("127.0.0.1", PORT, 2000);
	assert(socket_invalid != socket);

	r = socket_getsendtimeout(socket, &timeout);
	//printf("socket_getsendtimeout: %d, timeout=%u\n", r, timeout);
	while (-1 != socket_send(socket, msg2, sizeof(msg2), 0)); // fill send buffer

	aio_socket_init(1);
	aiosocket = aio_socket_create(socket, 0);
	assert(invalid_aio_socket != aiosocket);
	assert(0 == aio_socket_send(aiosocket, msg2, sizeof(msg2), onsend, NULL));

	//////////////////////////////////////////////////////////////////////////
	/// test 1
	timeout = 1;
	assert(0 == socket_setsendtimeout(socket, timeout));
	r = aio_socket_process(2000);
	printf("send[1] socket set send timeout [%d] => %s\n", r, 0==r?"failed" : "OK");
	if(1 == r) aio_socket_send(aiosocket, msg2, sizeof(msg2), onsend, NULL);

	//////////////////////////////////////////////////////////////////////////
	/// test 2
	assert(0 == socket_shutdown(socket, SHUT_WR));
	r = aio_socket_process(2000);
	printf("send[2] socket shutdown write [%d] => %s\n", r, 0==r?"failed" : "OK");
	if (1 == r) aio_socket_send(aiosocket, msg2, sizeof(msg2), onsend, NULL);

	//////////////////////////////////////////////////////////////////////////
	/// test 3
	assert(0 == socket_shutdown(socket, SHUT_RD));
	r = aio_socket_process(2000);
	printf("send[3] socket shutdown read [%d] => %s\n", r, 0==r?"failed" : "OK");
	if (1 == r) aio_socket_send(aiosocket, msg2, sizeof(msg2), onsend, NULL);

	//////////////////////////////////////////////////////////////////////////
	/// test 4
	assert(0 == socket_close(socket));
	r = aio_socket_process(2000);
	printf("send[4] socket close [%d] => %s\n", r, 0==r?"failed" : "OK");

	aio_socket_destroy(aiosocket, ondestroy, NULL);
	aio_socket_process(2000);
	aio_socket_clean();
	return 0;
}

void aio_socket_test(void)
{
	pthread_t thread;

	socket_init();
	thread_create(&thread, worker, NULL);
	system_sleep(1); // switch to worker thread

	aio_socket_recv_test();
	aio_socket_send_test();

	thread_destroy(thread);
	socket_cleanup();
}
