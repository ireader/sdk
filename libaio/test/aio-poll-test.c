#include "aio-poll.h"
#include "sockutil.h"
#include "sys/system.h"

#define TCP_IP	"127.0.0.1"
#define TCP_PORT 12345

struct aio_poll_test_t
{
	struct aio_poll_t* poll;
};

static void aio_poll_test_onrecv(int code, socket_t socket, int flags, void* param)
{
	int r;
	char msg[64];
	struct aio_poll_test_t* t;
	t = (struct aio_poll_test_t*)param;
	r = socket_recv(socket, msg, sizeof(msg), 0);
	printf("recv code: %d, msg: %.*s\n", code, r > 0 ? r : 0, msg);

	if (0 == code)
	{
		aio_poll_poll(t->poll, socket, AIO_POLL_IN, 10 * 1000, aio_poll_test_onrecv, t);
		socket_close(socket);
	}
}

static void aio_poll_test_onconnected(int code, socket_t socket, int flags, void* param)
{
	struct aio_poll_test_t* t;
	t = (struct aio_poll_test_t*)param;

	assert(0 == code);
	assert(AIO_POLL_OUT & flags);
	aio_poll_poll(t->poll, socket, AIO_POLL_IN, 10 * 1000, aio_poll_test_onrecv, t);
}

static void aio_poll_test_onaccept(int code, socket_t socket, int flags, void* param)
{
	socket_t c;
	socklen_t addrlen;
	struct sockaddr_storage addr;
	struct aio_poll_test_t* t;
	const char* msg = "Hello, I'm accept!";
	t = (struct aio_poll_test_t*)param;

	assert(0 == code);
	assert(AIO_POLL_IN & flags);
	c = socket_accept(socket, &addr, &addrlen);
	assert(strlen(msg) == socket_send(c, msg, strlen(msg), 0));
}

void aio_poll_test(void)
{
	socket_t tcp;
	socket_t c;
	struct sockaddr_in addr;
	struct aio_poll_test_t t;

	socket_init();
	tcp = socket_tcp_listen_ipv4(TCP_IP, TCP_PORT, 64);

	t.poll = aio_poll_create();
	aio_poll_poll(t.poll, tcp, AIO_POLL_IN, 10 * 1000, aio_poll_test_onaccept, &t);

	socket_addr_from_ipv4(&addr, TCP_IP, TCP_PORT);
	c = socket_tcp();	
	socket_connect(c, (struct sockaddr*) & addr, sizeof(addr));
	aio_poll_poll(t.poll, c, AIO_POLL_OUT, 10 * 1000, aio_poll_test_onconnected, &t);

#if defined(OS_RTOS)
	system_sleep(10 * 1000);
#else
	system_sleep(1000 * 1000);
#endif

	aio_poll_destroy(t.poll);
	socket_close(tcp);
	socket_cleanup();
}
