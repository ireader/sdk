#include "sys/sock.h"
#include "sys/thread.h"
#include "aio-socket.h"
#include "sockutil.h"

#define PORT 1937

static char s_recv_buffer[2 * 1024];
static char s_send_buffer[64 * 1024];

static void aio_socket_onrecv(void* param, int code, size_t bytes)
{
	int r;
	aio_socket_t aio = (aio_socket_t)param;
	r = aio_socket_recv(aio, s_recv_buffer, sizeof(s_recv_buffer), aio_socket_onrecv, aio);
	assert(0 == r);
}

static int STDCALL aio_socket_recv_test(void* param)
{
	socket_t socket;
	aio_socket_t aio;
	socket = socket_connect_host("127.0.0.1", PORT, 10000);
	aio = aio_socket_create(socket, 1);
	aio_socket_recv(aio, s_recv_buffer, sizeof(s_recv_buffer), aio_socket_onrecv, aio);
	
	while (1)
	{
		aio_socket_process(10);
	}

	aio_socket_destroy(aio, NULL, NULL);
	return 0;
}

void aio_socket_test5(void)
{
	int r;
	pthread_t thread;
	socket_t socket;
	socket_t client;
	struct sockaddr_storage addr;
	socklen_t len = sizeof(addr);

	socket_init();
	aio_socket_init(1);

	socket = socket_tcp_listen_ipv4(NULL, PORT, 64);
	thread_create(&thread, aio_socket_recv_test, NULL);

	client = socket_accept(socket, &addr, &len);
	while (1)
	{
		r = socket_send_all_by_time(client, s_send_buffer, sizeof(s_send_buffer), 0, 20 * 1000);
		assert(r >= 0);
	}
	socket_close(client);
	socket_close(socket);
	
	thread_destroy(thread);
	aio_socket_clean();
	socket_cleanup();
}
