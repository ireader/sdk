#include "aio-socket.h"
#include "sys/system.h"
#include "sys/thread.h"
#include "sockutil.h"
#include <errno.h>

static char rbuffer[8 * 1024];
static char wbuffer[2*1024*1024];

static void ondestroy(void* param)
{
	const char* msg = (const char*)param;
	printf("%s ondestroy\n", msg);
}

static void onconnect(void* param, int code)
{
	const char* msg = (const char*)param;
	printf("%s onconnect code: %d\n", msg, code);
}

static void onaccept(void* param, int code, socket_t socket, const struct sockaddr* addr, socklen_t addrlen)
{
	aio_socket_t* s = (aio_socket_t*)param;
	printf("onaccept code: %d, socket: %d\n", code, (int)socket);

	if (0 == code)
		aio_socket_accept(*s, onaccept, s);

	(void)addr;
	(void)addrlen;
}

static void onrecv(void* param, int code, size_t bytes)
{
	const char* msg = (const char*)param;
	printf("%s onrecv code: %d, bytes: %d\n", msg, code, (int)bytes);
}

static void onsend(void* param, int code, size_t bytes)
{
	const char* msg = (const char*)param;
	printf("%s onsend code: %d, bytes: %d\n", msg, code, (int)bytes);
}

static int STDCALL aio_thread(IN void* param)
{
	volatile int* running = (int*)param;
	while (*running)
	{
		aio_socket_process(1000);
	}

	return 0;
}

static void aio_socket_cancel_connect(void)
{
	socket_t socket;
	aio_socket_t aiosocket;
	struct sockaddr_in addr;

	socket = socket_tcp();
	assert(0 == socket_bind_any(socket, 0));
	aiosocket = aio_socket_create(socket, 1);
	socket_addr_from_ipv4(&addr, "127.0.0.1", 8008);
	assert(0 == aio_socket_connect(aiosocket, (struct sockaddr*)&addr, (socklen_t)sizeof(addr), onconnect, (void*)__FUNCTION__));
	system_sleep(1000);
	aio_socket_destroy(aiosocket, ondestroy, (void*)__FUNCTION__);
}

static void aio_socket_cancel_recv(void)
{
	socket_t socket;
	aio_socket_t aiosocket;

	socket = socket_connect_host("127.0.0.1", 8008, 5000);
	aiosocket = aio_socket_create(socket, 1);
	assert(0 == aio_socket_recv(aiosocket, rbuffer, sizeof(rbuffer), onrecv, (void*)__FUNCTION__));
	system_sleep(1000);
	aio_socket_destroy(aiosocket, ondestroy, (void*)__FUNCTION__);
}

static void aio_socket_cancel_send(void)
{
	socket_t socket;
	aio_socket_t aiosocket;

	socket = socket_connect_host("127.0.0.1", 8008, 5000);
	assert(socket_invalid != socket);
	while (-1 != socket_send(socket, wbuffer, sizeof(wbuffer), 0)); // fill send buffer
	aiosocket = aio_socket_create(socket, 1);
	assert(0 == aio_socket_send(aiosocket, wbuffer, sizeof(wbuffer), onsend, (void*)__FUNCTION__));
	system_sleep(1000);
	aio_socket_destroy(aiosocket, ondestroy, (void*)__FUNCTION__);
}

static void aio_socket_cancel_rw(void)
{
	socket_t socket;
	aio_socket_t aiosocket;

	socket = socket_connect_host("127.0.0.1", 8008, 5000);
	assert(socket_invalid != socket);
	while (-1 != socket_send(socket, wbuffer, sizeof(wbuffer), 0)); // fill send buffer
	aiosocket = aio_socket_create(socket, 1);
	assert(0 == aio_socket_send(aiosocket, wbuffer, sizeof(wbuffer), onsend, (void*)__FUNCTION__));
	assert(0 == aio_socket_recv(aiosocket, rbuffer, sizeof(rbuffer), onrecv, (void*)__FUNCTION__));
	system_sleep(1000);
	aio_socket_destroy(aiosocket, ondestroy, (void*)__FUNCTION__);
}

void aio_socket_test_cancel(void)
{
	int running;
	socket_t socket;
	aio_socket_t aiosocket;
	pthread_t thread;

	static char* msg = "aio_socket_test_accept";

	running = 1;
	socket_init();
	aio_socket_init(1);

	// local service
	socket = socket_tcp_listen_ipv4(NULL, 8008, 1);
	assert(socket_invalid != socket);
	aiosocket = aio_socket_create(socket, 1);
	assert(invalid_aio_socket != aiosocket);

	// don't accept
	aio_socket_cancel_connect();
	aio_socket_cancel_connect();
	
	thread_create(&thread, aio_thread, &running);
	system_sleep(1000);

	// start accept socket
	aio_socket_accept(aiosocket, onaccept, &aiosocket);

	aio_socket_cancel_recv();
	system_sleep(1000);
	aio_socket_cancel_send();
	system_sleep(1000);
	aio_socket_cancel_rw();
	system_sleep(1000);
	
	// cancel accept
	aio_socket_destroy(aiosocket, ondestroy, msg);
	system_sleep(1000);

	running = 0;
	thread_destroy(thread);
	aio_socket_clean();
	socket_cleanup();
}
