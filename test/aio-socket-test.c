#include "cstringext.h"
#include "tcpserver.h"
#include "aio-socket.h"
#include "sys/system.h"
#include "sys/thread.h"
#include <errno.h>

#if defined(OS_WINDOWS)
#define EINPROGRESS             WSAEINPROGRESS
#endif

static char msg2[200*1024*1024];

static int STDCALL worker(IN void* param)
{
	socket_t socket;
	socket_t client;
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);

	param = param;
	socket = tcpserver_create(NULL, 8008, 64);

	//while(1)
	{
		client = socket_accept(socket, (struct sockaddr*)&addr, &len);
	}

//	socket_close(client);
	socket_close(socket);
	return 0;
}

static void onrecv(void* param, int code, size_t bytes)
{
	param, code, bytes;
	//printf("aio_socket_recv_test onrecv: code=%d, bytes=%d\n", code, bytes);
}

static int aio_socket_recv_test(void)
{
	int r;
	socket_t socket;
	aio_socket_t aiosocket;
	char msg[1024];
	size_t timeout;

	socket = socket_tcp();
	r = socket_connect_ipv4_by_time(socket, "127.0.0.1", 8008, 5000);
	if(0 != r) printf("socket_connect_ipv4_by_time: %d\n", r);

	r = socket_getrecvtimeout(socket, &timeout);
	//printf("socket_getrecvtimeout: %d, timeout=%u\n", r, timeout);

	//timeout = 2;
	//r = socket_setrecvtimeout(socket, timeout);
	//printf("socket_setrecvtimeout: %d\n", r);

	aio_socket_init(4);
	aiosocket = aio_socket_create(socket, 0);
	r = aio_socket_recv(aiosocket, msg, sizeof(msg), onrecv, NULL);
	//printf("aio_socket_recv_test recv: %d\n", r);

	//////////////////////////////////////////////////////////////////////////
	/// test 1
	timeout = 2;
	r = socket_setrecvtimeout(socket, timeout);
	r = aio_socket_process(5000);
	printf("recv[1] socket set receive timeout [%d]%s\n", r, 0==r?"failed" : "OK");

	//////////////////////////////////////////////////////////////////////////
	/// test 2
	r = socket_shutdown(socket, SHUT_WR);
	if(0 != r) printf("aio_socket_recv_test shutdown(write): %d\n", r);
	r = aio_socket_process(5000);
	printf("recv[2] socket shutdown write [%d]%s\n", r, 0==r?"failed" : "OK");

	//////////////////////////////////////////////////////////////////////////
	/// test 3
	r = socket_shutdown(socket, SHUT_RD);
	if(0 != r) printf("aio_socket_recv_test shutdown(read): %d\n", r);
	r = aio_socket_process(5000);
	printf("recv[3] socket shutdown read [%d]%s\n", r, 0==r?"failed" : "OK");

	//////////////////////////////////////////////////////////////////////////
	/// test 4
	r = socket_close(socket);
	if(0 != r) printf("aio_socket_recv_test close(socket): %d\n", r);
	r = aio_socket_process(5000);
	printf("recv[4] socket close [%d]%s\n", r, 0==r?"failed" : "OK");

	aio_socket_destroy(aiosocket);
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

	socket = socket_tcp();
	r = socket_connect_ipv4_by_time(socket, "127.0.0.1", 8008, 5000);
	if(0 != r) printf("socket_connect_ipv4_by_time: %d\n", r);

	r = socket_getsendtimeout(socket, &timeout);
	//printf("socket_getsendtimeout: %d, timeout=%u\n", r, timeout);

	aio_socket_init(4);
	aiosocket = aio_socket_create(socket, 0);

	r = aio_socket_send(aiosocket, msg2, sizeof(msg2), onsend, NULL);
	while(r != 0 && (EAGAIN == r || EINPROGRESS == r) )
		r = aio_socket_send(aiosocket, msg2, sizeof(msg2), onsend, NULL);
	if(0 != r) printf("aio_socket_send_test send: %d\n", r);

	//////////////////////////////////////////////////////////////////////////
	/// test 1
	timeout = 2;
	r = socket_setsendtimeout(socket, timeout);
	if(0 != r) printf("socket_setsendtimeout: %d\n", r);
	r = aio_socket_process(5000);
	printf("send[1] socket set send timeout [%d]%s\n", r, 0==r?"failed" : "OK");

	//////////////////////////////////////////////////////////////////////////
	/// test 2
	r = socket_shutdown(socket, SHUT_WR);
	if(0 != r) printf("aio_socket_send_test shutdown(write): %d\n", r);
	r = aio_socket_process(5000);
	printf("send[2] socket shutdown write [%d]%s\n", r, 0==r?"failed" : "OK");

	//////////////////////////////////////////////////////////////////////////
	/// test 3
	r = socket_shutdown(socket, SHUT_RD);
	if(0 != r) printf("aio_socket_send_test shutdown(read): %d\n", r);
	r = aio_socket_process(5000);
	printf("send[3] socket shutdown read [%d]%s\n", r, 0==r?"failed" : "OK");

	//////////////////////////////////////////////////////////////////////////
	/// test 4
	r = socket_close(socket);
	if(0 != r) printf("aio_socket_send_test close(socket): %d\n", r);
	r = aio_socket_process(5000);
	printf("send[4] socket close [%d]%s\n", r, 0==r?"failed" : "OK");

	aio_socket_destroy(aiosocket);
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
#if !defined(OS_WINDOWS)
	/// windows send always ok
	aio_socket_send_test();
#endif

	thread_destroy(thread);
	socket_cleanup();
}
