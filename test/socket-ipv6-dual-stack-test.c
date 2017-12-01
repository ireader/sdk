#include "sockutil.h"
#include "sys/thread.h"
#include "sys/system.h"
#include <assert.h>
#include <stdio.h>

#define PORT 8000
#define HELLO "Hello World!"

static int STDCALL socket_tcp_server(void* param)
{
	int i, n;
	char msg[128];
	socket_t s, c;
	struct sockaddr_storage addr;
	socklen_t addrlen;

	s = socket_tcp_listen_ipv6(NULL, PORT, SOMAXCONN, 1);
	assert(socket_invalid != s);

	for(i = 0; i < 2; i++)
	{
		addrlen = sizeof(addr);
		c = socket_accept(s, &addr, &addrlen);
		assert(socket_invalid != c);
		n = socket_recv_by_time(c, msg, sizeof(msg), 0, 1000);
		socket_send_all_by_time(c, msg, n, 0, 1000); // echo
		socket_close(c);
	}

	socket_close(s);
	return 0; (void)param;
}

static int STDCALL socket_udp_server(void* param)
{
	int i, n;
	char msg[128];
	socket_t s;
	struct sockaddr_storage addr;
	socklen_t addrlen;

	s = socket_udp_bind_ipv6(NULL, PORT, 1);
	assert(socket_invalid != s);

	for(i = 0; i < 2; i++)
	{
		addrlen = sizeof(addr);
		n = socket_recvfrom(s, msg, sizeof(msg), 0, (struct sockaddr*)&addr, &addrlen);
		socket_sendto(s, msg, n, 0, (struct sockaddr*)&addr, addrlen); // echo
	}

	socket_close(s);
	return 0; (void)param;
}

static int socket_tcp_client(int ipv6)
{
	int r, ret;
	socket_t s;
	char msg[64];

	ret = -1;
	s = socket_connect_host(ipv6 ? "::1" : "127.0.0.1", PORT, 1000);
	if (socket_invalid != s)
	{
		r = socket_send_all_by_time(s, HELLO, strlen(HELLO), 0, 1000);
		if (r == (int)strlen(HELLO))
		{
			r = socket_recv_by_time(s, msg, sizeof(msg), 0, 1000);
			if ((int)strlen(HELLO) == r && 0 == memcmp(msg, HELLO, r))
				ret = 0;
		}
	}

	socket_close(s);
	return ret;
}

static int socket_udp_client(int ipv6)
{
	int r, ret;
	socket_t s;
	char msg[64];
	struct sockaddr_storage addr;
	socklen_t addrlen;
	
	socket_addr_from(&addr, &addrlen, ipv6 ? "::1" : "127.0.0.1", PORT);

	ret = -1;
	s = ipv6 ? socket_udp_ipv6() : socket_udp();
	if (socket_invalid != s)
	{
		r = socket_sendto(s, HELLO, strlen(HELLO), 0, (struct sockaddr*)&addr, addrlen);
		if (r == (int)strlen(HELLO))
		{
			addrlen = sizeof(addr);
			r = socket_recvfrom(s, msg, sizeof(msg), 0, (struct sockaddr*)&addr, &addrlen);
			if ((int)strlen(HELLO) == r && 0 == memcmp(msg, HELLO, r))
				ret = 0;
		}
	}

	socket_close(s);
	return ret;
}

void socket_ipv6_dual_stack_test(void)
{
	pthread_t t[2];
	socket_init();

	thread_create(&t[0], socket_tcp_server, NULL);
	thread_create(&t[1], socket_udp_server, NULL);

	system_sleep(1000);

	assert(0 == socket_tcp_client(0));
	assert(0 == socket_tcp_client(1));
	assert(0 == socket_udp_client(0));
	assert(0 == socket_udp_client(1));

	thread_destroy(t[0]);
	thread_destroy(t[1]);

	socket_cleanup();
	printf("IPv4 dual-stack test ok\n");
}
