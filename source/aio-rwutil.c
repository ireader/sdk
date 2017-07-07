#include "aio-rwutil.h"

#if defined(OS_WINDOWS)
#define iov_base buf  
#define iov_len  len 
#endif

struct aio_socket_ptr_t
{
	aio_socket_t socket;

	union
	{
		aio_onsend onsend;
		aio_onrecv onrecv;
	} u;
	void* param; // callback parameter

	socket_bufvec_t* vec;
	size_t count;

	socket_bufvec_t __vec[1];
	size_t __n; // reserved internal use, don't change it value
};

static void aio_socket_onrecv_v(void* param, int code, size_t bytes)
{
	size_t i, n;
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

		if (i < ptr->count)
		{
			n = bytes - n;
			ptr->vec[i].iov_len -= n;
			ptr->vec[i].iov_base = (char*)ptr->vec[i].iov_base + n;
			ptr->vec += i;
			ptr->count -= i;
			code = aio_socket_recv_v(ptr->socket, ptr->vec, ptr->count, aio_socket_onrecv_v, ptr);
		}
		else
		{
			ptr->u.onrecv(ptr->param, code, ptr->__n);
		}
	}

	if (0 != code)
		ptr->u.onrecv(ptr->param, code, ptr->__n);
}

static void aio_socket_onsend_v(void* param, int code, size_t bytes)
{
	size_t i, n;
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

		if (i < ptr->count)
		{
			n = bytes - n;
			ptr->vec[i].iov_len -= n;
			ptr->vec[i].iov_base = (char*)ptr->vec[i].iov_base + n;
			ptr->vec += i;
			ptr->count -= i;
			code = aio_socket_send_v(ptr->socket, ptr->vec, ptr->count, aio_socket_onsend_v, ptr);
		}
		else
		{
			ptr->u.onsend(ptr->param, code, ptr->__n);
		}
	}

	if (0 != code)
		ptr->u.onsend(ptr->param, code, ptr->__n);
}

int aio_socket_recv_all(struct aio_socket_rw_t* rw, aio_socket_t socket, void* buffer, size_t bytes, aio_onrecv proc, void* param)
{
	struct aio_socket_ptr_t* ptr;
	ptr = (struct aio_socket_ptr_t*)rw;
	ptr->__vec[0].iov_len = bytes;
	ptr->__vec[0].iov_base = (char*)buffer;
	return aio_socket_recv_v_all(rw, socket, ptr->__vec, 1, proc, param);
}

int aio_socket_recv_v_all(struct aio_socket_rw_t* rw, aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecv proc, void* param)
{
	struct aio_socket_ptr_t* ptr;
	ptr = (struct aio_socket_ptr_t*)rw;
	ptr->socket = socket;
	ptr->vec = vec;
	ptr->count = n;
	ptr->u.onrecv = proc;
	ptr->param = param;
	ptr->__n = 0;
	return aio_socket_recv_v(ptr->socket, ptr->vec, ptr->count, aio_socket_onrecv_v, ptr);
}

int aio_socket_send_all(struct aio_socket_rw_t* rw, aio_socket_t socket, const void* buffer, size_t bytes, aio_onsend proc, void* param)
{
	struct aio_socket_ptr_t* ptr;
	ptr = (struct aio_socket_ptr_t*)rw;
	ptr->__vec[0].iov_len = bytes;
	ptr->__vec[0].iov_base = (char*)buffer;
	return aio_socket_send_v_all(rw, socket, ptr->__vec, 1, proc, param);
}

int aio_socket_send_v_all(struct aio_socket_rw_t* rw, aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onsend proc, void* param)
{
	struct aio_socket_ptr_t* ptr;
	ptr = (struct aio_socket_ptr_t*)rw;
	ptr->socket = socket;
	ptr->vec = vec;
	ptr->count = n;
	ptr->u.onsend = proc;
	ptr->param = param;
	ptr->__n = 0;
	return aio_socket_send_v(ptr->socket, ptr->vec, ptr->count, aio_socket_onsend_v, ptr);
}
