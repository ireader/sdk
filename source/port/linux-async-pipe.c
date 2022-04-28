#if defined(OS_LINUX)
#include "port/async-pipe.h"
#include "sys/atomic.h"
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <aio.h>
#include <signal.h>
typedef struct
{
	int rpipe; // read only
	int wpipe; // write only
	struct aiocb aio;

	void* param;
	async_pipe_onread read;
	async_pipe_onwrite write;
	int len;

#if defined(_DEBUG) || defined(DEBUG)
	int32_t ref;
#endif
} LinuxPipe;

// pipe(fd)
// pipe_server("fd[0].fd[1]")
async_pipe_t async_pipe_server(const char* name)
{
	LinuxPipe* o;
	o = (LinuxPipe*)malloc(sizeof(LinuxPipe));
	if(!o)
		return NULL;

	memset(o, 0, sizeof(LinuxPipe));
	if(2 != sscanf(name, "%d.%d", &o->rpipe, &o->wpipe))
	{
		free(o);
		return 0;
	}
	return o;
}

async_pipe_t async_pipe_open(const char* name)
{
	return async_pipe_server(name);
}

int async_pipe_close(async_pipe_t pipe)
{
	LinuxPipe* o = (LinuxPipe*)pipe;
	assert(0 == o->ref);
	close(o->rpipe);
	close(o->wpipe);
	free(o);
	return 0;
}

static void aio_onread(sigval_t sigval)
{
	int e, n;
	LinuxPipe* o = (LinuxPipe*)sigval.sival_ptr;
	assert(0==atomic_decrement32(&o->ref));

	e = aio_error(&o->aio);
	n = aio_return(&o->aio);
	if(0 == e && n > 0)
	{
		if(n < o->aio.aio_nbytes)
		{
			o->aio.aio_nbytes -= n;
			o->aio.aio_buf = ((char*)(o->aio.aio_buf)) + n;
			assert(1==atomic_increment32(&o->ref));
			n = aio_read(&o->aio);
			if(0 != n)
			{
				assert(0==atomic_decrement32(&o->ref));
				o->read(o->param, n, 0);
			}
		}
		else
		{
			assert(n == o->aio.aio_nbytes);
			o->read(o->param, 0, o->len);
		}
	}
	else
	{
		o->read(o->param, 0==e?EPIPE:e, 0);
	}
}

static void aio_onwrite(sigval_t sigval)
{
	int e, n;
	LinuxPipe* o = (LinuxPipe*)sigval.sival_ptr;
	assert(0==atomic_decrement32(&o->ref));

	e = aio_error(&o->aio);
	n = aio_return(&o->aio);
	if(0 == e && n > 0)
	{
		if(n < o->aio.aio_nbytes)
		{
			o->aio.aio_nbytes -= n;
			o->aio.aio_buf = ((char*)(o->aio.aio_buf)) + n;
			assert(1==atomic_increment32(&o->ref));
			n = aio_write(&o->aio);
			if(0 != n)
			{
				assert(0==atomic_decrement32(&o->ref));
				o->write(o->param, n, 0);
			}
		}
		else
		{
			assert(n == o->aio.aio_nbytes);
			o->write(o->param, 0, o->len);
		}
	}
	else
	{
		o->write(o->param, 0==e?EPIPE:e, 0);
	}
}

int async_pipe_read(async_pipe_t pipe, void* msg, int len, async_pipe_onread callback, void* param)
{
	LinuxPipe* o = (LinuxPipe*)pipe;
	o->read = callback;
	o->param = param;
	o->len = len;

	o->aio.aio_fildes = o->rpipe;
	o->aio.aio_nbytes = len;
	o->aio.aio_buf = msg;
	o->aio.aio_sigevent.sigev_notify = SIGEV_THREAD;
	o->aio.aio_sigevent.sigev_notify_function = aio_onread;
	o->aio.aio_sigevent.sigev_notify_attributes = NULL;
	o->aio.aio_sigevent.sigev_value.sival_ptr = o;

	assert(1==atomic_increment32(&o->ref));
	if(0 != aio_read(&o->aio))
	{
		assert(0==atomic_decrement32(&o->ref));
		return (int)errno;
	}
	return 0;
}

int async_pipe_write(async_pipe_t pipe, const void* msg, int len, async_pipe_onwrite callback, void* param)
{
	LinuxPipe* o = (LinuxPipe*)pipe;
	o->write = callback;
	o->param = param;
	o->len = len;

	o->aio.aio_fildes = o->wpipe;
	o->aio.aio_nbytes = len;
	o->aio.aio_buf = (void*)msg;
	o->aio.aio_sigevent.sigev_notify = SIGEV_THREAD;
	o->aio.aio_sigevent.sigev_notify_function = aio_onwrite;
	o->aio.aio_sigevent.sigev_notify_attributes = NULL;
	o->aio.aio_sigevent.sigev_value.sival_ptr = o;

	assert(1==atomic_increment32(&o->ref));
	if(0 != aio_write(&o->aio))
	{
		assert(0==atomic_decrement32(&o->ref));
		return (int)errno;
	}
	return 0;
}

#endif