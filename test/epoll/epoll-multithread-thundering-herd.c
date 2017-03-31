#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define THREAD 32
#define PORT 8888

static int server;
static void* OnThread(void* p)
{
	int r, epfd;
	struct epoll_event event;
	struct sockaddr_storage storage;
	socklen_t socklen = sizeof(storage);

	memset(&event, 0, sizeof(event));
	event.events = EPOLLIN;

	epfd = epoll_create(64);
	r = epoll_ctl(epfd, EPOLL_CTL_ADD, server, &event);
	memset(&event, 0, sizeof(event));
	r = epoll_wait(epfd, &event, 1, -1);
	//r = accept(server, (struct sockaddr*)&storage, &socklen);
	printf("[%d] epoll_wait: %d, event: %u\n", (int)p, r, event.events);
	close(epfd);
}

static void epoll_multithread_thundering_herd_test(void)
{
	pid_t pid = -1;
	int r, i;
	int client;
	struct sockaddr_in in;

	memset(&in, 0, sizeof(in));
	in.sin_family = AF_INET;
	in.sin_port = htons(PORT);
	in.sin_addr.s_addr = INADDR_ANY;

	server = socket(AF_INET, SOCK_STREAM, 0);
	r = bind(server, (struct sockaddr*)&in, sizeof(in));
	r = listen(server, 64);

	for (i = 0; i < THREAD; i++)
	{
		pthread_t thread;
		pthread_create(&thread, NULL, OnThread, (void*)i);
		pthread_detach(thread);
	}

	r = getchar();
	memset(&in, 0, sizeof(in));
	in.sin_family = AF_INET;
	in.sin_port = htons(PORT);
	in.sin_addr.s_addr = inet_addr("127.0.0.1");

	client = socket(AF_INET, SOCK_STREAM, 0);
	r = connect(client, (struct sockaddr*)&in, sizeof(in));
	printf("client connect: %d\n", r);
	printf("%s exit\n", __FUNCTION__);
}

int main(int argc, char* argv[])
{
	epoll_multithread_thundering_herd_test();
	return 0;
}
