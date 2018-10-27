#ifndef _aio_client_h_
#define _aio_client_h_

#include "sys/sock.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct aio_client_handler_t
{
	/// aio client destroy
	void (*ondestroy)(void* param);

	/// connect notify(maybe call mutil-times)
	/// @param[in] param user-defined pointer
	void (*onconn)(void* param);

	/// @param[in] param user-defined pointer
	/// @param[in] code 0-ok, other-error
	/// @param[in] bytes recv data length, valid if code is 0
	void (*onrecv)(void* param, int code, size_t bytes);

	/// @param[in] param user-defined pointer
	/// @param[in] code 0-ok, other-error
	/// @param[in] bytes sent data length, valid if code is 0
	void (*onsend)(void* param, int code, size_t bytes);
};

typedef struct aio_client_t aio_client_t;

aio_client_t* aio_client_create(const char* host, int port, struct aio_client_handler_t* handler, void* param);
int aio_client_destroy(aio_client_t* client);

/// start/stop connection(optional, recv/send will auto start connection)
int aio_client_connect(aio_client_t* client);
int aio_client_disconnect(aio_client_t* client);

/// Recv data
/// @param[in] data user memory address, MUST BE VALID until onrecv
/// @param[in] bytes data length in byte
int aio_client_recv(aio_client_t* client, void* data, size_t bytes);

/// Recv data
/// @param[in] vec buffer vector, MUST BE VALID until onrecv
/// @param[in] n vector count
int aio_client_recv_v(aio_client_t* client, socket_bufvec_t *vec, int n);

/// Send data to peer
/// @param[in] data use data send to peer, MUST BE VALID until onsend
/// @param[in] bytes data length in byte
/// @return 0-ok, other-error
int aio_client_send(aio_client_t* client, const void* data, size_t bytes);

/// Send data to peer
/// @param[in] vec data vector, MUST BE VALID until onsend
/// @param[in] n vector count
/// @return 0-ok, other-error
int aio_client_send_v(aio_client_t* client, socket_bufvec_t *vec, int n);

/// @param[in] conn connect/recv/send timeout(millisecond), default 2min, 0-infinite
void aio_client_settimeout(aio_client_t* client, int conn, int recv, int send);
void aio_client_gettimeout(aio_client_t* client, int* conn, int* recv, int* send);

#if defined(__cplusplus)
}
#endif
#endif /* !_aio_client_h_ */
