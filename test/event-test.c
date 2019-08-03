#include "cstringext.h"
#include "sys/event.h"
#include "sys/thread.h"
#include "sys/system.h"
#include <stdio.h>

static event_t ev1;
static event_t ev2;

static int STDCALL thread0(void* param)
{
	uint64_t t1, t2;
	param = param;
	printf("event thread0 - 0\n");
	t1 = system_clock();
	assert(WAIT_TIMEOUT == event_timewait(&ev1, 3000));
	t2 = system_clock();
	assert(t2 - t1 >= 2999);
	printf("thread0 - timewait ok\n");

	assert(0 == event_signal(&ev1));
	printf("thread0 - signal 0 ok\n");
	assert(0 == event_wait(&ev2));
	printf("thread0 - wait 0 ok\n");
	assert(0 == event_signal(&ev1));
	printf("thread0 - signal 1 ok\n");
	assert(0 == event_wait(&ev2));
	printf("thread0 - wait 1 ok\n");
	assert(0 == event_signal(&ev1));
	printf("thread0 - signal 2 ok\n");
	assert(0 == event_wait(&ev2));
	printf("thread0 - wait 2 ok\n");
	assert(0 == event_signal(&ev1));
	printf("thread0 - signal 3 ok\n");
	assert(0 == event_wait(&ev2));
	printf("thread0 - wait 3 ok\n");
	assert(0 == event_signal(&ev1));
	printf("thread0 - signal 4 ok\n");
	assert(0 == event_wait(&ev2));
	printf("thread0 - wait 4 ok\n");
	assert(0 == event_signal(&ev1));
	printf("thread0 - signal 5 ok\n");
	assert(0 == event_wait(&ev2));
	printf("thread0 - wait 5 ok\n");
	printf("thread0 - exit\n");
	return 0;
}

static int STDCALL thread1(void* param)
{
    param = param;
	printf("event thread1 - 0\n");
	assert(0 == event_wait(&ev1));
	printf("thread1 - wait 0 ok\n");
	assert(0 == event_signal(&ev2));
	printf("thread1 - signal 0 ok\n");
	assert(0 == event_wait(&ev1));
	printf("thread1 - wait 1 ok\n");
	assert(0 == event_signal(&ev2));
	printf("thread1 - signal 1 ok\n");
	assert(0 == event_wait(&ev1));
	printf("thread1 - wait 2 ok\n");
	assert(0 == event_signal(&ev2));
	printf("thread1 - signal 2 ok\n");
	assert(0 == event_wait(&ev1));
	printf("thread1 - wait 3 ok\n");
	assert(0 == event_signal(&ev2));
	printf("thread1 - signal 3 ok\n");
	assert(0 == event_wait(&ev1));
	printf("thread1 - wait 4 ok\n");
	assert(0 == event_signal(&ev2));
	printf("thread1 - signal 4 ok\n");
	assert(0 == event_wait(&ev1));
	printf("thread1 - wait 5 ok\n");
	assert(0 == event_signal(&ev2));
	printf("thread1 - signal 5 ok\n");
	printf("thread1 - exit\n");
	return 0;
}

void event_test(void)
{
	pthread_t threads[2];
	assert(0 == event_create(&ev1));
	assert(0 == event_create(&ev2));

	event_signal(&ev1);
	event_reset(&ev1);

	thread_create(&threads[0], thread0, NULL);
	thread_create(&threads[1], thread1, NULL);

	thread_destroy(threads[0]);
	thread_destroy(threads[1]);

	assert(0 == event_destroy(&ev1));
	assert(0 == event_destroy(&ev2));
}
