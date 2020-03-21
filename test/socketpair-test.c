#include "sys/sock.h"
#include "port/socketpair.h"

static const char* s_send1 = "from s[0]";
static const char* s_send2 = "from s[1]";

#if defined(OS_WINDOWS)
void socketpair_tcp_test(void)
{
	char buf[128];
	socket_t s[2];
	assert(0 == socketpair(AF_INET6, SOCK_STREAM, 0, s));

	assert((int)strlen(s_send1) == socket_send(s[0], s_send1, strlen(s_send1), 0));
	assert((int)strlen(s_send1) == socket_recv(s[1], buf, sizeof(buf), 0));
	assert(0 == memcmp(s_send1, buf, strlen(s_send1)));

	assert((int)strlen(s_send2) == socket_send(s[1], s_send2, strlen(s_send2), 0));
	assert((int)strlen(s_send2) == socket_recv(s[0], buf, sizeof(buf), 0));
	assert(0 == memcmp(s_send2, buf, strlen(s_send2)));

	socket_close(s[0]);
	socket_close(s[1]);
}

void socketpair_udp_test(void)
{
	char buf[128];
	socket_t s[2];
	assert(0 == socketpair(AF_INET, SOCK_DGRAM, 0, s));

	assert((int)strlen(s_send1) == socket_send(s[0], s_send1, strlen(s_send1), 0));
	assert((int)strlen(s_send1) == socket_recv(s[1], buf, sizeof(buf), 0));
	assert(0 == memcmp(s_send1, buf, strlen(s_send1)));

	assert((int)strlen(s_send2) == socket_send(s[1], s_send2, strlen(s_send2), 0));
	assert((int)strlen(s_send2) == socket_recv(s[0], buf, sizeof(buf), 0));
	assert(0 == memcmp(s_send2, buf, strlen(s_send2)));

	socket_close(s[0]);
	socket_close(s[1]);
}

#else

void socketpair_unix_stream_test(void)
{
    char buf[128];
    socket_t s[2];
    assert(0 == socketpair(AF_UNIX, SOCK_STREAM, 0, s));

    assert((int)strlen(s_send1) == socket_send(s[0], s_send1, strlen(s_send1), 0));
    assert((int)strlen(s_send1) == socket_recv(s[1], buf, sizeof(buf), 0));
    assert(0 == memcmp(s_send1, buf, strlen(s_send1)));

    assert((int)strlen(s_send2) == socket_send(s[1], s_send2, strlen(s_send2), 0));
    assert((int)strlen(s_send2) == socket_recv(s[0], buf, sizeof(buf), 0));
    assert(0 == memcmp(s_send2, buf, strlen(s_send2)));

    socket_close(s[0]);
    socket_close(s[1]);
}

void socketpair_unix_dgram_test(void)
{
    char buf[128];
    socket_t s[2];
    assert(0 == socketpair(AF_UNIX, SOCK_DGRAM, 0, s));

    assert((int)strlen(s_send1) == socket_send(s[0], s_send1, strlen(s_send1), 0));
    assert((int)strlen(s_send1) == socket_recv(s[1], buf, sizeof(buf), 0));
    assert(0 == memcmp(s_send1, buf, strlen(s_send1)));

    assert((int)strlen(s_send2) == socket_send(s[1], s_send2, strlen(s_send2), 0));
    assert((int)strlen(s_send2) == socket_recv(s[0], buf, sizeof(buf), 0));
    assert(0 == memcmp(s_send2, buf, strlen(s_send2)));

    socket_close(s[0]);
    socket_close(s[1]);
}
#endif

void socketpair_test(void)
{
	socket_init();
#if defined(OS_WINDOWS)
	socketpair_tcp_test();
	socketpair_udp_test();
#else
    socketpair_unix_stream_test();
    socketpair_unix_dgram_test();
#endif
	socket_cleanup();
}
