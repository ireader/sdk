#include "cstringext.h"
#include "sys/sock.h"
#include "sys/process.h"
#include <stdio.h>

static socket_t socket_create_and_bind(const char* ip, int port, int backlog)
{
	int r;
	socket_t server;
	struct sockaddr_in addr;

	server = socket_tcp();
	if(socket_error == server)
		return socket_invalid;

	// reuse addr
	r = socket_setreuseaddr(server, 1);

	// bind
	if(!ip || 0 == ip[0])
	{
		r = socket_bind_any(server, port);
	}
	else
	{
		r = socket_addr_ipv4(&addr, ip, port);
		if(0 == r)
			r = socket_bind(server, (struct sockaddr*)&addr, sizeof(addr));
	}

	// listen
	if(0 == r)
		r = socket_listen(server, backlog);

	if(0 != r)
	{
		socket_close(server);
		return socket_invalid;
	}

	return server;
}

int main(int argc, char* argv[])
{
	return 0;
}
