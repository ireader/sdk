#include <stdio.h>
#include <ctype.h>

// base library
// gcc -I../include -DOS_LINUX main.c atomic-test.c semaphore-test.c spinlock-test.c event-test.c locker-test.c -o test -lpthread -ldl -lrt
// gcc -I../include -DOS_LINUX main.c atomic-test.c semaphore-test.c spinlock-test.c event-test.c locker-test.c aio-socket-test.c ../source/aio-socket-epoll.c -o test -lpthread -ldl -lrt

void locker_test(void);
void atomic_test(void);
void spinlock_test(void);
void event_test(void);
void semaphore_test(void);

void stack_test(void);

void url_test(void);
void unicode_test(void);
void utf8codec_test(void);
void thread_pool_test(void);
void systimer_test(void);
void aio_socket_test(void);
void aio_socket_test2(void);
void aio_socket_test3(void);
void ip_route_test(void);

// librtsp
#if defined(OS_WINDOWS)
void rtsp_header_range_test(void);
void rtsp_header_session_test(void);
void rtsp_header_rtp_info_test(void);
void rtsp_header_transport_test(void);
void sdp_test(void);
void sdp_a_fmtp_test(void);
void sdp_a_rtpmap_test(void);
#endif

int main(int argc, char* argv[])
{
	locker_test();
	atomic_test();
	spinlock_test();
	event_test();
#if !defined(OS_MAC)
	semaphore_test();
#endif

	stack_test();

	aio_socket_test2();
	aio_socket_test3();

#if defined(OS_WINDOWS)
//	rtsp_header_range_test();
	rtsp_header_session_test();
	rtsp_header_rtp_info_test();
	rtsp_header_transport_test();

	sdp_test();
	sdp_a_fmtp_test();
	sdp_a_rtpmap_test();

	url_test();
	unicode_test();
	utf8codec_test();
	thread_pool_test();

	ip_route_test();

	//aio_socket_test();
	//systimer_test();
	system("pause");
#endif
	return 0;
}
