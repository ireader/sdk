#include "cstringext.h"
#include "sys/sock.h"
#include "sys/system.h"
#include "sys/thread.h"
#include "sys/process.h"
#include "aio-socket.h"
#include "thread-pool.h"
#include "aio-thread-pool.h"
#include <stdio.h>
#include <stdlib.h>

static thread_pool_t s_thpool;
static char s_buffer[2*1024*1024];

static void AIOWorker(void* param)
{
	int r;
	while(1)
	{
		r = aio_socket_process(10);
	}
}

static void OnRead(void* param, int code, int bytes);
static void OnWrite(void* param, int code, int bytes)
{
	void* ptr;
	aio_socket_t aio;

	aio = *(aio_socket_t*)param;
	ptr = (aio_socket_t*)param + 1;

	printf("[%u] OnWrite[%p] code=%d, bytes=%d\n", thread_self(), aio, code, bytes);

	memset(ptr, 0, 1024);
	aio_socket_recv(aio, ptr, 1023, OnRead, param);
}

static void OnRead(void* param, int code, int bytes)
{
	void* ptr;
	aio_socket_t aio;

	aio = *(aio_socket_t*)param;
	ptr = (aio_socket_t*)param + 1;

	printf("[%u]OnRead[%p] code=%d, bytes=%d, recv=%s\n", thread_self(), aio, code, bytes, (char*)ptr);

	aio_socket_send(aio, s_buffer, sizeof(s_buffer), OnWrite, param);
}

static void OnAccept(void* param, int code, socket_t socket, const char* ip, int port)
{
	void* ptr;
	aio_socket_t aio;
	aio_socket_t server = (aio_socket_t)param;

	printf("OnAccept ==> %d; %s:%d\n", code, ip, port);
	if(0 != code)
		return;

	ptr = malloc(1024+sizeof(aio));
	aio = aio_socket_create(socket, 1);

	*(aio_socket_t*)ptr = aio;

	memset((aio_socket_t*)ptr+1, 0, 1024);
	aio_socket_recv(aio, (aio_socket_t*)ptr+1, 1023, OnRead, ptr);

	// continue accept
	aio_socket_accept(server, OnAccept, server);
}

static socket_t Listen(int port)
{
	int r;
	socket_t server;

	server = socket_tcp();
	if(socket_error == server)
		return socket_invalid;

	// reuse addr
	r = socket_setreuseaddr(server, 1);

	// bind
	r = socket_bind_any(server, (unsigned short)port);

	// listen
	r = socket_listen(server, 64);

	return server;
}

int main(int argc, char* argv[])
{
	char c;
	int cpu = system_getcpucount();
	socket_t server;
	aio_socket_t aioserver;

	memset(s_buffer, 'A', sizeof(s_buffer)-1);

	aio_socket_init(cpu);
	s_thpool = thread_pool_create(cpu, cpu, cpu*2);

	while(cpu > 0)
	{
		thread_pool_push(s_thpool, AIOWorker, NULL);
		--cpu;
	}

	server = Listen(50000);
	aioserver = aio_socket_create(server, 1);
	aio_socket_accept(aioserver, OnAccept, aioserver);

	printf("server listen at: %d\n", 50000);
	for(c = getchar(); 'q' != c; c = getchar())
	{
		switch(c)
		{
		case 'c': // close socket
			aio_socket_destroy(aioserver);
			break;

		default:
			printf("unknown command.\nc : close socket\n");
		}
	}

	aio_socket_destroy(aioserver);
	thread_pool_destroy(s_thpool);
	aio_socket_clean();
	return 0;
}
