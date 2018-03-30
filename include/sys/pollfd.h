#ifndef _platform_poll_h_
#define _platform_poll_h_

#if !defined(OS_WINDOWS)
#include <poll.h>
#else
#include <Winsock2.h>

#if(_WIN32_WINNT < 0x0600)

#define POLLRDNORM  0x0100
#define POLLRDBAND  0x0200
#define POLLIN      (POLLRDNORM | POLLRDBAND)
#define POLLPRI     0x0400

#define POLLWRNORM  0x0010
#define POLLOUT     (POLLWRNORM)
#define POLLWRBAND  0x0020

#define POLLERR     0x0001
#define POLLHUP     0x0002
#define POLLNVAL    0x0004

struct pollfd {
    SOCKET  fd;
    SHORT   events;
    SHORT   revents;
};
#endif

/// @param[in] timeout select timeout in ms
/// @return 0-timeout, >0-available fds, <0-socket_error(by socket_geterror())
static inline int poll(IN struct pollfd* fds, IN int count, IN int timeout)
{
#if(_WIN32_WINNT >= 0x0600) // vista
    return WSAPoll(fds, count, timeout);
#else
    int i, r;
    fd_set rfds;
    fd_set wfds;
    fd_set efds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);
    for (i = 0; i < count; i++)
    {
        assert(socket_invalid != fds[i].fd); // linux: FD_SET error
        if (fds[i].events & POLLIN)
            FD_SET(fds[i].fd, &rfds);
        if (fds[i].events & POLLOUT)
            FD_SET(fds[i].fd, &wfds);
        FD_SET(fds[i].fd, &efds);
    }

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    r = select(count, &rfds, &wfds, &efds, timeout < 0 ? NULL : &tv);
    if (r <= 0)
        return r;

    for (i = 0; i < count; i++)
    {
        fds[i].revents = 0;
        if (FD_ISSET(fds[i].fd, &rfds))
            fds[i].revents |= POLLIN;
        if (FD_ISSET(fds[i].fd, &wfds))
            fds[i].revents |= POLLOUT;
        if (FD_ISSET(fds[i].fd, &efds))
            fds[i].revents |= POLLERR;
    }
    return r;
#endif
}
#endif

#endif /* !_platform_poll_h_ */
