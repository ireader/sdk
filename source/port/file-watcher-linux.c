#if defined(OS_LINUX)
#include "port/file-watcher.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <poll.h>

static inline int events_to_inotify(int events)
{
	int flags;
	flags = (events & FILE_WATCHER_EVENT_OPEN) ? IN_OPEN : 0;
	flags |= (events & FILE_WATCHER_EVENT_CLOSE) ? IN_CLOSE : 0;
	flags |= (events & FILE_WATCHER_EVENT_READ) ? IN_ACCESS : 0;
	flags |= (events & FILE_WATCHER_EVENT_WRITE) ? IN_MODIFY : 0;
	flags |= (events & FILE_WATCHER_EVENT_CHMOD) ? IN_ATTRIB : 0;
	flags |= (events & FILE_WATCHER_EVENT_CREATE) ? IN_CREATE : 0;
	flags |= (events & FILE_WATCHER_EVENT_DELETE) ? IN_DELETE | IN_DELETE_SELF : 0;
	flags |= (events & FILE_WATCHER_EVENT_RENAME) ? IN_MOVE | IN_MOVE_SELF : 0;
	return flags;
}

static inline int events_from_inotify(int flags)
{
	int events;
	events = (flags & IN_OPEN) ? FILE_WATCHER_EVENT_OPEN : 0;
	events |= (flags & IN_CLOSE) ? FILE_WATCHER_EVENT_CLOSE : 0;
	events |= (flags & IN_ACCESS) ? FILE_WATCHER_EVENT_READ : 0;
	events |= (flags & IN_MODIFY) ? FILE_WATCHER_EVENT_WRITE : 0;
	events |= (flags & IN_ATTRIB) ? FILE_WATCHER_EVENT_CHMOD : 0;
	events |= (flags & IN_CREATE) ? FILE_WATCHER_EVENT_CREATE : 0;
	events |= (flags & (IN_DELETE|IN_DELETE_SELF)) ? FILE_WATCHER_EVENT_DELETE : 0;
	events |= (flags & (IN_MOVE|IN_MOVE_SELF)) ? FILE_WATCHER_EVENT_RENAME : 0;
	return events;
}

file_watcher_t file_watcher_create(void)
{
	int fd;
	fd = inotify_init1(IN_NONBLOCK);
	if (-1 == fd)
		return NULL;
	return (void*)(intptr_t)fd;
}

void file_watcher_destroy(file_watcher_t watcher)
{
	int fd;
	fd = (int)(intptr_t)watcher;
	if (-1 == fd || 0 == fd)
		return;
	close(fd);
}

void* file_watcher_add(file_watcher_t watcher, const char* fileordir, int events)
{
	int fd, wd, flags;
	fd = (int)(intptr_t)watcher;
	if (-1 == fd || 0 == fd)
		return NULL;

	flags = events_to_inotify(events);
	wd = inotify_add_watch(fd, fileordir, flags);
	if (-1 == wd)
		return NULL;
	return (void*)(intptr_t)wd;
}

int file_watcher_delete(file_watcher_t watcher, void* file)
{
	int fd, wd;
	fd = (int)(intptr_t)watcher;
	wd = (int)(intptr_t)file;
	if (-1 == fd || 0 == fd || -1 == wd || 0 == wd)
		return -ENOENT;

	return inotify_rm_watch(fd, wd);
}

int file_watcher_process(file_watcher_t watcher, int timeout, int (*onnotify)(void* param, void* file, int events, const char* name, int bytes), void* param)
{
	int fd;
	int r, n, events;
	struct pollfd fds[1];
	struct inotify_event *evt;
	char buf[1024];

	fd = (int)(intptr_t)watcher;
	if (-1 == fd || 0 == fd)
		return -ENOENT;

	fds[0].fd = fd;
	fds[0].events = POLLIN;
	fds[0].revents = 0;
	r = poll(fds, 1, timeout);
	while (-1 == r && (EINTR == errno || EAGAIN == errno))
		r = poll(fds, 1, timeout);
	if (r <= 0)
		return 0 == r ? -ETIMEDOUT : -1;

	n = read(fd, buf, sizeof(buf));
	if (n <= 0)
		return -1;

	for (evt = (struct inotify_event*)buf; n >= sizeof(struct inotify_event) + evt->len;)
	{
		if (0 == (evt->mask & IN_MOVED_FROM)) // ignore old name
		{
			events = events_from_inotify(evt->mask);
			r = onnotify(param, (void*)(intptr_t)evt->wd, events, evt->name, strlen(evt->name));
			if (0 != r)
				return r;
		}

		n -= sizeof(struct inotify_event) + evt->len;
		evt = (struct inotify_event*)((char*)evt + sizeof(struct inotify_event) + evt->len);
	}
	
	return 0;
}
#endif
