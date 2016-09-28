#include "cstringext.h"
#include "sys/sock.h"
#include "sys/system.h"
#include "time64.h"
#include <stdio.h>

static char reply[128];

int main(int argc, char* argv[])
{
	socket_t tcp;
	const char *msg = "hello server";
	const char *host = "127.0.0.1";
	int port = 2012;
	int work = 10000;
	int i, r, n, bytes;
	time64_t lt, lt1;

	for(i = 1; i < argc; i++)
	{
		if(0 == strcmp("-h", argv[i]))
		{
			if(i + 1 >= argc) exit(1);
			host = argv[++i];
		}
		else if(0 == strcmp("-p", argv[i]))
		{

			if(i + 1 >= argc) exit(1);
			port = atoi(argv[++i]);
		}
		else if(0 == strcmp("-w", argv[i]))
		{
			if(i+1 >= argc) exit(1);
			work = atoi(argv[++i]);
		}
		else if(0 == strcmp("-m", argv[i]))
		{
			if(i+1 >= argc) exit(1);
			msg = argv[++i];
		}
	}

	socket_init();
	tcp = socket_connect_host(host, (unsigned short)port);
	if(socket_invalid == tcp)
	{
		printf("socket connect %s:%d error: %d/%d\n", host, port, errno, socket_geterror());
		return 1;
	}

	socket_setnonblock(tcp, 1);

	n = strlen(msg);
	lt = time64_now();
	for(i = 0; i < work; i++)
	{
		r = socket_send(tcp, msg, n, 0);
		if(r < 0)
		{
			printf("socket send[%d] error: %d/%d\n", i, r, socket_geterror());
			return 1;
		}

		bytes = 0;
		do
		{
			r = socket_recv(tcp, reply, sizeof(reply)-1, 0);
			if(r > 0) bytes += r;
		} while(r > 0);

		if(r < 0 && WSAGetLastError()!=WSAEWOULDBLOCK)
		{
			printf("socket receive[%d] error: %d/%d\n", i, r, socket_geterror());
			return 1;
		}
		else
		{
			printf("socket receive[%d] bytes=%d: %d/%d\n", i, bytes);
		}

		system_sleep(1000);
	}
	
	lt1 = time64_now();
	printf("tcp[%s:%d] send/recv %d time: %ll\n", host, port, work, lt1-lt);

	socket_close(tcp);
	socket_cleanup();
	return 0;
}
