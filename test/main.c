#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#if defined(OS_LINUX)
#include <signal.h>
#endif

// base library
// gcc -I../include -DOS_LINUX main.c atomic-test.c semaphore-test.c spinlock-test.c event-test.c locker-test.c -o test -lpthread -ldl -lrt
// gcc -I../include -DOS_LINUX main.c atomic-test.c semaphore-test.c spinlock-test.c event-test.c locker-test.c aio-socket-test.c ../source/aio-socket-epoll.c -o test -lpthread -ldl -lrt

#if defined(OS_WINDOWS) || defined(OS_MAC)
#define HTTP_TEST
#endif

void string_test(void);

void heap_test(void);
void rbtree_test(void);
void timer_test(void);

void socket_test(void);
void locker_test(void);
void atomic_test(void);
void atomic_test2(void);
void spinlock_test(void);
void event_test(void);
void semaphore_test(void);
void onetime_test(void);

void bits_test(void);
void stack_test(void);
void time64_test(void);
void base64_test(void);
void bitmap_test(void);
void hweight_test(void);
void ring_buffer_test(void);
void channel_test(void);

void unicode_test(void);
void uri_parse_test(void);
void utf8codec_test(void);
void thread_pool_test(void);
void task_queue_test(void);
void systimer_test(void);
void aio_socket_test(void);
void aio_socket_test2(void);
void aio_socket_test3(void);
void aio_socket_test4(void);
void aio_socket_test_cancel(void);
void ip_route_test(void);
void onetime_test(void);
void socketpair_test(void);
void aio_poll_test(void);

#if defined(HTTP_TEST)
void http_test(void);
#endif

#if defined(SDP_TEST)
void sdp_test(void);
#endif

int main(int argc, char* argv[])
{
#if defined(OS_LINUX)
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, 0);
	sigaction(SIGPIPE, &sa, 0);
#endif

	string_test();

	heap_test();
	rbtree_test();
	channel_test();
	timer_test();

	socket_test();
	locker_test();
	atomic_test();
#if defined(ATOMIC_TEST)
	atomic_test2();
#endif
	spinlock_test();
	event_test();
#if !defined(OS_MAC)
	semaphore_test();
#endif
	onetime_test();
	socketpair_test();

	bits_test();
	stack_test();
	time64_test();
	base64_test();
	bitmap_test();
	hweight_test();
	ring_buffer_test();

	uri_parse_test();

#if defined(HTTP_TEST)
	http_test();
#endif

	thread_pool_test();
	task_queue_test();

	ip_route_test();

	aio_poll_test();
    aio_socket_test_cancel();
    aio_socket_test();
    aio_socket_test2();
    aio_socket_test3();
    aio_socket_test4();

#if defined(OS_WINDOWS)
	unicode_test();
	utf8codec_test();
	systimer_test();

	system("pause");
#endif

    return 0;
}
