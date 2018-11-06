#include "sys/sock.h"

void socket_opt_dontfrag_test(void)
{
	static const char* msg = "Hello UDP";

	int flag;
	socket_t udp;
	struct sockaddr_storage addr;
	socklen_t addrlen;

	udp = socket_udp();
	assert(0 == socket_getdontfrag(udp, &flag));
    printf("socket default don't fragment flag: %d\n", flag);
    assert(0 == socket_setdontfrag(udp, 1));
    assert(0 == socket_getdontfrag(udp, &flag) && 1 == flag);
    socket_close(udp);
}
