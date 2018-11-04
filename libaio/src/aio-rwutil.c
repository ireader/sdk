#include "aio-rwutil.h"
#include "sys/system.h"
#include "aio-recv.h"
#include "aio-send.h"
#include <errno.h>

#if defined(OS_WINDOWS)
#define iov_base buf  
#define iov_len  len 
#endif

struct aio_socket_ptr_t
{
	aio_socket_t socket;

	union
	{
		struct aio_recv_t recv;
		struct aio_send_t send;
	} u;
	int timeout;
	uint64_t clock;

	union
	{
		aio_onrecv onrecv;
		aio_onsend onsend;
	} on;
	void* param;

	socket_bufvec_t* vec;
	int count;

	socket_bufvec_t __vec[1];
	size_t __n; // reserved internal use, don't change it value
};

static void aio_socket_onrecv_v(void* param, int code, size_t bytes)
{
	int i;
	size_t n;
	struct aio_socket_ptr_t* ptr;
	ptr = (struct aio_socket_ptr_t*)param;
	if (0 == code)
	{
		ptr->__n += bytes;
		for (i = 0, n = 0; i < ptr->count; i++)
		{
			if (n + ptr->vec[i].iov_len > bytes)
				break;
			n += ptr->vec[i].iov_len;
		}

		if (i == ptr->count)
		{
			ptr->on.onrecv(ptr->param, code, ptr->__n);
		}
		else if (ptr->clock + ptr->timeout < system_clock())
		{
			code = ETIMEDOUT;
		}
		else
		{
			n = bytes - n;
			ptr->vec[i].iov_len -= n;
			ptr->vec[i].iov_base = (char*)ptr->vec[i].iov_base + n;
			ptr->vec += i;
			ptr->count -= i;
			code = aio_recv_v(&ptr->u.recv, ptr->timeout, ptr->socket, ptr->vec, ptr->count, aio_socket_onrecv_v, ptr);
		}
	}

	if (0 != code)
		ptr->on.onrecv(ptr->param, code, ptr->__n);
}

static void aio_socket_onsend_v(void* param, int code, size_t bytes)
{
	int i;
	size_t n;
	struct aio_socket_ptr_t* ptr;
	ptr = (struct aio_socket_ptr_t*)param;
	if (0 == code)
	{
		ptr->__n += bytes;
		for (i = 0, n = 0; i < ptr->count; i++)
		{
			if (n + ptr->vec[i].iov_len > bytes)
				break;
			n += ptr->vec[i].iov_len;
		}

		if (i == ptr->count)
		{
			ptr->on.onsend(ptr->param, code, ptr->__n);
		}
		else if (ptr->clock + ptr->timeout < system_clock())
		{
			code = ETIMEDOUT;
		}
		else
		{
			n = bytes - n;
			ptr->vec[i].iov_len -= n;
			ptr->vec[i].iov_base = (char*)ptr->vec[i].iov_base + n;
			ptr->vec += i;
			ptr->count -= i;
			code = aio_send_v(&ptr->u.send, ptr->timeout, ptr->socket, ptr->vec, ptr->count, aio_socket_onsend_v, ptr);
		}
	}

	if (0 != code)
		ptr->on.onsend(ptr->param, code, ptr->__n);
}

int aio_socket_recv_all(struct aio_socket_rw_t* rw, int timeout, aio_socket_t socket, void* buffer, size_t bytes, aio_onrecv proc, void* param)
{
	struct aio_socket_ptr_t* ptr;
	ptr = (struct aio_socket_ptr_t*)rw;
	ptr->__vec[0].iov_len = bytes;
	ptr->__vec[0].iov_base = (char*)buffer;
	return aio_socket_recv_v_all(rw, timeout, socket, ptr->__vec, 1, proc, param);
}

int aio_socket_recv_v_all(struct aio_socket_rw_t* rw, int timeout, aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecv proc, void* param)
{
	struct aio_socket_ptr_t* ptr;
	ptr = (struct aio_socket_ptr_t*)rw;
	ptr->clock = system_clock();
	ptr->timeout = timeout;
	ptr->socket = socket;
	ptr->on.onrecv = proc;
	ptr->param = param;
	ptr->vec = vec;
	ptr->count = n;
	ptr->__n = 0;
	return aio_recv_v(&ptr->u.recv, timeout, ptr->socket, ptr->vec, ptr->count, aio_socket_onrecv_v, ptr);
}

int aio_socket_send_all(struct aio_socket_rw_t* rw, int timeout, aio_socket_t socket, const void* buffer, size_t bytes, aio_onsend proc, void* param)
{
	struct aio_socket_ptr_t* ptr;
	ptr = (struct aio_socket_ptr_t*)rw;
	ptr->__vec[0].iov_len = bytes;
	ptr->__vec[0].iov_base = (char*)buffer;
	return aio_socket_send_v_all(rw, timeout, socket, ptr->__vec, 1, proc, param);
}

int aio_socket_send_v_all(struct aio_socket_rw_t* rw, int timeout, aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onsend proc, void* param)
{
	struct aio_socket_ptr_t* ptr;
	ptr = (struct aio_socket_ptr_t*)rw;
	ptr->clock = system_clock();
	ptr->timeout = timeout;
	ptr->socket = socket;
	ptr->on.onsend = proc;
	ptr->param = param;
	ptr->vec = vec;
	ptr->count = n;
	ptr->__n = 0;
	return aio_send_v(&ptr->u.send, timeout, ptr->socket, ptr->vec, ptr->count, aio_socket_onsend_v, ptr);
}
