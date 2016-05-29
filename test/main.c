#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "sys/sock.h"

// base library
// gcc -I../include -DOS_LINUX main.c atomic-test.c semaphore-test.c spinlock-test.c event-test.c locker-test.c -o test -lpthread -ldl -lrt
// gcc -I../include -DOS_LINUX main.c atomic-test.c semaphore-test.c spinlock-test.c event-test.c locker-test.c aio-socket-test.c ../source/aio-socket-epoll.c -o test -lpthread -ldl -lrt

#if defined(OS_WINDOWS) || defined(OS_MAC)
#define HTTP_TEST
#define RTSP_TEST
#define SDP_TEST
#endif

void socket_test(void);
void locker_test(void);
void atomic_test(void);
void atomic_test2(void);
void spinlock_test(void);
void event_test(void);
void semaphore_test(void);

void stack_test(void);
void time64_test(void);

void url_test(void);
void unicode_test(void);
void utf8codec_test(void);
void thread_pool_test(void);
void task_queue_test(void);
void systimer_test(void);
void aio_socket_test(void);
void aio_socket_test2(void);
void aio_socket_test3(void);
void aio_socket_test4(void);
void ip_route_test(void);

#if defined(HTTP_TEST)
void http_test(void);
#endif

#if defined(RTSP_TEST)
void rtsp_test(void);
#endif

#if defined(SDP_TEST)
void sdp_test(void);
#endif

int main(int argc, char* argv[])
{
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
	stack_test();
	socket_test();

    url_test();
    time64_test();

#if defined(HTTP_TEST)
	http_test();
#endif

#if defined(RTSP_TEST)
	rtsp_test();
#endif

	thread_pool_test();
	task_queue_test();

	ip_route_test();
    
    aio_socket_test2();
    aio_socket_test3();
	aio_socket_test4();
	aio_socket_test();

#if defined(SDP_TEST)
	sdp_test();
#endif

#if defined(OS_WINDOWS)
    unicode_test();
    utf8codec_test();
	systimer_test();

	system("pause");
#endif

    return 0;
}
