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
void url_test(void);
void unicode_test(void);
void utf8codec_test(void);
void thread_pool_test(void);
void systimer_test(void);
void sdp_test(void);
void aio_socket_test(void);

int main(int argc, char* argv[])
{
	locker_test();
	atomic_test();
	spinlock_test();
	event_test();
	semaphore_test();
	aio_socket_test();
	//url_test();
	//unicode_test();
	//utf8codec_test();
	//thread_pool_test();
	//systimer_test();
	//sdp_test();
	system("pause");
	return 0;
}
