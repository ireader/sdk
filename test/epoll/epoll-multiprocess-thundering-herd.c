#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define N_CHILD 32
#define PORT 8888

static void epoll_multiprocess_thundering_herd_test(void)
{
	pid_t pid = -1;
	int r, i, epfd;
	int server, client;
	struct sockaddr_in in;
	
	memset(&in, 0, sizeof(in));
	in.sin_family = AF_INET;
	in.sin_port = htons(PORT);
	in.sin_addr.s_addr = INADDR_ANY;

	server = socket(AF_INET, SOCK_STREAM, 0);
	r = bind(server, (struct sockaddr*)&in, sizeof(in));
	r = listen(server, 64);

	epfd = epoll_create(64);

	for (i = 0; i < N_CHILD && 0 != pid; i++)
	{
		pid = fork();
		if (pid < 0)
		{
			printf("fork(%d) error: %d\n", i, errno);
		}
		if (0 == pid)
		{
			// child process

			struct epoll_event event;
			struct sockaddr_storage storage;
			socklen_t socklen = sizeof(storage);

			memset(&event, 0, sizeof(event));
			event.events = EPOLLIN;
			r = epoll_ctl(epfd, EPOLL_CTL_ADD, server, &event);
			
			memset(&event, 0, sizeof(event));
			r = epoll_wait(epfd, &event, 1, -1);
			//r = accept(server, (struct sockaddr*)&storage, &socklen);
			printf("[%d] epoll_wait: %d, event: %u\n", getpid(), r, event.events);
			close(epfd);
		}
		else
		{
			// parent process
			// DO NOTHING
			printf("child(%d) launch\n", pid);
		}
	}

	if (0 != pid)
	{
		r = getchar();
		memset(&in, 0, sizeof(in));
		in.sin_family = AF_INET;
		in.sin_port = htons(PORT);
		in.sin_addr.s_addr = inet_addr("127.0.0.1");

		client = socket(AF_INET, SOCK_STREAM, 0);
		r = connect(client, (struct sockaddr*)&in, sizeof(in));
		printf("client connect: %d\n", r);

		for (i = 0; i < N_CHILD; i++)
		{
			wait();
		}

		printf("%s exit\n", __FUNCTION__);
	}
}

int main(int argc, char* argv[])
{
	epoll_multiprocess_thundering_herd_test();
	return 0;
}
