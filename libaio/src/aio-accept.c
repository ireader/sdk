#include "aio-accept.h"
#include "sys/locker.h"
#include <assert.h>
#include <stdlib.h>

struct aio_accept_t
{
	locker_t locker;
	aio_socket_t socket;

	aio_onaccept onaccpet;
	void* param;

	aio_ondestroy ondestroy;
	void* param2;
};

static void aio_accept_onclient(void* param, int code, socket_t socket, const struct sockaddr* addr, socklen_t addrlen)
{
	int r;
	struct aio_accept_t* aio;
	aio = (struct aio_accept_t*)param;

	r = code;
	if (0 == code)
	{
		// continue accept
		locker_lock(&aio->locker);
		if (invalid_aio_socket != aio->socket)
			r = aio_socket_accept(aio->socket, aio_accept_onclient, aio);
		else
			r = -1; // destroy
		locker_unlock(&aio->locker);
		
		aio->onaccpet(aio->param, code, socket, addr, addrlen);
	}

	if (0 != r)
	{
		// accept error or user cancel(destroy)
		aio->onaccpet(aio->param, r, 0, NULL, 0);
	}
}

static void aio_accept_ondestroy(void* param)
{
	struct aio_accept_t* aio;
	aio = (struct aio_accept_t*)param;
	if (aio->ondestroy)
		aio->ondestroy(aio->param);

	assert(invalid_aio_socket == aio->socket);
	locker_destroy(&aio->locker);
	free(aio);
}

void* aio_accept_start(socket_t socket, aio_onaccept onaccept, void* param)
{
	struct aio_accept_t* aio;

	if (NULL == onaccept)
		return NULL;

	aio = (struct aio_accept_t*)calloc(1, sizeof(*aio));
	if (NULL == aio)
		return NULL;

	aio->param = param;
	aio->onaccpet = onaccept;
	locker_create(&aio->locker);
	aio->socket = aio_socket_create(socket, 1);
	if (0 != aio_socket_accept(aio->socket, aio_accept_onclient, aio))
	{
		aio_accept_stop(aio, NULL, NULL);
		return NULL;
	}

	return aio;
}

int aio_accept_stop(void* p, aio_ondestroy ondestroy, void* param)
{
	aio_socket_t socket;
	struct aio_accept_t* aio;
	aio = (struct aio_accept_t*)p;
	aio->ondestroy = ondestroy;
	aio->param2 = param;

	socket = aio->socket;
	locker_lock(&aio->locker);
	aio->socket = invalid_aio_socket;
	locker_unlock(&aio->locker);

	return aio_socket_destroy(socket, aio_accept_ondestroy, aio);
}
