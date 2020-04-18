#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define THREAD 10
#define PORT 88888

static int epfd;
static int server;
static void* OnThread(void* p)
{
	int r;
	//struct epoll_event event;
	//struct sockaddr_storage storage;
	//socklen_t socklen = sizeof(storage);
	size_t stacksize = 0;

	void* x;
	x = malloc(10);
	//free(x);
	//pthread_attr_t attr;
	//pthread_getattr_np(pthread_self(), &attr);
	//pthread_attr_getstacksize(&attr, &stacksize);
	//printf("thread stack size: %u\n", (unsigned int)stacksize);
	//pthread_attr_destroy(&attr);

	//memset(&event, 0, sizeof(event));
	//r = epoll_wait(epfd, &event, 1, -1);
	//printf("[%d] epoll_wait: %d/%d, event: %u\n", (int)p, r, errno,  event.events);
	//r = accept(server, (struct sockaddr*)&storage, &socklen);
	//printf("[%d] accept: %d/%d\n", (int)p, r, errno);
	usleep(10 * 1000 * 1000);
	//close(r);
    free(x);
}

static void epoll_thundering_herd_test(void)
{
	int r, i;
	pthread_t thread;
	//int client;
	//struct sockaddr_in in;
	//struct epoll_event event;

	//memset(&in, 0, sizeof(in));
	//in.sin_family = AF_INET;
	//in.sin_port = htons(PORT);
	//in.sin_addr.s_addr = INADDR_ANY;

	//server = socket(AF_INET, SOCK_STREAM, 0);
	//r = bind(server, (struct sockaddr*)&in, sizeof(in));
	//r = listen(server, 64);

	//memset(&event, 0, sizeof(event));
	//event.events = EPOLLIN;
	//epfd = epoll_create(64);
	//r = epoll_ctl(epfd, EPOLL_CTL_ADD, server, &event);

	for (i = 0; i < THREAD; i++)
	{
		//pthread_attr_t attr;
		//pthread_attr_init(&attr);
		//pthread_attr_setstacksize(&attr, 128*1024);
		pthread_create(&thread, NULL, OnThread, (void*)i);
		pthread_detach(thread);
		//pthread_attr_destroy(&attr);
	}

	while (scanf("%d", &r))
	{
		printf("scanf %d\n", r);
		//memset(&in, 0, sizeof(in));
		//in.sin_family = AF_INET;
		//in.sin_port = htons(PORT);
		//in.sin_addr.s_addr = inet_addr("127.0.0.1");

		//client = socket(AF_INET, SOCK_STREAM, 0);
		//r = connect(client, (struct sockaddr*)&in, sizeof(in));
		//printf("client connect: %d\n", r);
		//usleep(10 * 1000 * 1000);
		//close(client);
	}
	printf("%s exit\n", __FUNCTION__);

	//close(epfd);
}

int main(int argc, char* argv[])
{
	epoll_thundering_herd_test();
	return 0;
}
