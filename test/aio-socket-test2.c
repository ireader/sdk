#include "cstringext.h"
#include "tcpserver.h"
#include "aio-socket.h"
#include <errno.h>

#define PORT 23456

struct aio_client_t
{
	aio_socket_t socket;
	int cc;
	size_t len;
	char msg[64];
	socket_bufvec_t vec[2];
};

struct aio_server_t
{
	aio_socket_t socket;
	int cc;
	size_t len;
	char msg[64];
	socket_bufvec_t vec[2];
};

static const char *s_cmsg1 = "aio-client-request";
static const char *s_cmsg2 = "aio-client-request-v";
static const char *s_smsg1 = "aio-server-reply";
static const char *s_smsg2 = "aio-server-reply-v";

//////////////////////////////////////////////////////////////////////////
/// Server
/// Accept -> OnAccept(Recv) -> OnRecv(Send) -> OnSend(RecvVec) -> OnRecvVec(SendVec) -> OnSendVec(Recv)
///                     ^                                                                            |
///                     |----------------------<--------------------<--------------------<-----------+
///
static void aio_server_onrecv(void* param, int code, size_t bytes);
static void aio_server_onsend_v(void* param, int code, size_t bytes)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes == strlen(s_smsg2));

	printf("aio-server round: %d\n", ++server->cc);

	server->len = 0;
	memset(server->msg, 0, sizeof(server->msg));
	r = aio_socket_recv(server->socket, server->msg, strlen(s_cmsg1), aio_server_onrecv, param);
	assert(0 == r);
}

static void aio_server_onrecv_v(void* param, int code, size_t bytes)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes <= strlen(s_cmsg2));

	server->len += bytes;
	if(bytes < strlen(s_cmsg2))
	{
		if(server->len >= sizeof(server->msg)/2)
		{
			socket_setbufvec(server->vec, 0, server->msg+server->len, sizeof(server->msg)-server->len);
			r = aio_socket_recv_v(server->socket, server->vec, 1, aio_server_onrecv_v, param);
			assert(0 == r);
		}
		else
		{
			socket_setbufvec(server->vec, 0, server->msg+server->len, sizeof(server->msg)/2-server->len);
			socket_setbufvec(server->vec, 1, server->msg+sizeof(server->msg)/2, sizeof(server->msg)/2);
			r = aio_socket_recv_v(server->socket, server->vec, 2, aio_server_onrecv_v, param);
			assert(0 == r);
		}
	}
	else
	{
		assert(0 == strcmp(s_cmsg2, server->msg));
		socket_setbufvec(server->vec, 0, (void*)s_smsg2, strlen(s_smsg2)/2);
		socket_setbufvec(server->vec, 1, (void*)(s_smsg2+strlen(s_smsg2)/2), strlen(s_smsg2)/2);
		r = aio_socket_send_v(server->socket, server->vec, 2, aio_server_onsend_v, param);
		assert(0 == r);
	}
}

static void aio_server_onsend(void* param, int code, size_t bytes)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes == strlen(s_smsg1));

	server->len = 0;
	memset(server->msg, 0, sizeof(server->msg));
	socket_setbufvec(server->vec, 0, server->msg, sizeof(server->msg)/2);
	socket_setbufvec(server->vec, 1, server->msg+sizeof(server->msg)/2, sizeof(server->msg)/2);
	r = aio_socket_recv_v(server->socket, server->vec, 2, aio_server_onrecv_v, param);
	assert(0 == r);
}

static void aio_server_onrecv(void* param, int code, size_t bytes)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes <= strlen(s_cmsg1));

	server->len += bytes;
	if(bytes < strlen(s_cmsg1))
	{
		r = aio_socket_recv(server->socket, server->msg+server->len, strlen(s_cmsg1)-server->len, aio_server_onrecv, param);
		assert(0 == r);
	}
	else
	{
		assert(0 == strcmp(s_cmsg1, server->msg));

		r = aio_socket_send(server->socket, s_smsg1, strlen(s_smsg1), aio_server_onsend, param);
		assert(0 == r);
	}
}

static void aio_server_onaccept(void* param, int code, socket_t socket, const char* ip, int port)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;

	assert(0 == code);
	printf("aio_server_onaccept: %s:%d\n", ip, port);

	server->len = 0;
	memset(server->msg, 0, sizeof(server->msg));
	server->socket = aio_socket_create(socket, 1);
	r = aio_socket_recv(server->socket, server->msg, strlen(s_cmsg1), aio_server_onrecv, param);
	assert(0 == r);
}

//////////////////////////////////////////////////////////////////////////
/// Client
/// Connect -> OnConnect(Send) -> OnSend(Recv) -> OnRecv(SendVec) -> OnSendVec(RecvVec) -> OnRecvVec(Send)
///                       ^                                                                            |
///                       |----------------------<--------------------<--------------------<-----------+
///
static void aio_client_onsend(void* param, int code, size_t bytes);
static void aio_client_onrecv_v(void* param, int code, size_t bytes)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes <= strlen(s_smsg2));

	client->len += bytes;
	if(bytes < strlen(s_smsg2))
	{
		if(client->len >= sizeof(client->msg)/2)
		{
			socket_setbufvec(client->vec, 0, client->msg+client->len, sizeof(client->msg)-client->len);
			r = aio_socket_recv_v(client->socket, client->vec, 1, aio_client_onrecv_v, param);
			assert(0 == r);
		}
		else
		{
			socket_setbufvec(client->vec, 0, client->msg+client->len, sizeof(client->msg)/2-client->len);
			socket_setbufvec(client->vec, 1, client->msg+sizeof(client->msg)/2, sizeof(client->msg)/2);
			r = aio_socket_recv_v(client->socket, client->vec, 2, aio_client_onrecv_v, param);
			assert(0 == r);
		}
	}
	else
	{
		assert(0 == strcmp(s_smsg2, client->msg));

		printf("aio-client round: %d\n", ++client->cc);
		r = aio_socket_send(client->socket, s_cmsg1, strlen(s_cmsg1), aio_client_onsend, param);
		assert(0 == r);
	}
}

static void aio_client_onsend_v(void* param, int code, size_t bytes)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes <= strlen(s_cmsg2));

	client->len = 0;
	memset(client->msg, 0, sizeof(client->msg));

	socket_setbufvec(client->vec, 0, client->msg, sizeof(client->msg)/2);
	socket_setbufvec(client->vec, 1, client->msg+sizeof(client->msg)/2, sizeof(client->msg)/2);
	r = aio_socket_recv_v(client->socket, client->vec, 2, aio_client_onrecv_v, param);
	assert(0 == r);
}

static void aio_client_onrecv(void* param, int code, size_t bytes)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes <= strlen(s_smsg1));

	client->len += bytes;
	if(bytes < strlen(s_smsg1))
	{
		r = aio_socket_recv(client->socket, client->msg+client->len, strlen(s_smsg1)-client->len, aio_client_onrecv, param);
		assert(0 == r);
	}
	else
	{
		assert(0 == strcmp(s_smsg1, client->msg));

		socket_setbufvec(client->vec, 0, (void*)s_cmsg2, strlen(s_cmsg2)/2);
		socket_setbufvec(client->vec, 1, (void*)(s_cmsg2+strlen(s_cmsg2)/2), strlen(s_cmsg2)/2);
		r = aio_socket_send_v(client->socket, client->vec, 2, aio_client_onsend_v, param);
		assert(0 == r);
	}
}

static void aio_client_onsend(void* param, int code, size_t bytes)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes == strlen(s_cmsg1));

	client->len = 0;
	memset(client->msg, 0, sizeof(client->msg));
	r = aio_socket_recv(client->socket, client->msg, strlen(s_smsg1), aio_client_onrecv, param);
	assert(0 == r);
}

static void aio_client_onconnect(void* param, int code)
{
	int r;

	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code);

	printf("aio-client round: %d\n", ++client->cc);
	r = aio_socket_send(client->socket, s_cmsg1, strlen(s_cmsg1), aio_client_onsend, param);
	assert(0 == r);
}

void aio_socket_test2(void)
{
	int r;
	socket_t tcp;
	aio_socket_t socket;
	struct aio_client_t client;
	struct aio_server_t server;

	aio_socket_init(1);

	memset(&client, 0, sizeof(client));
	memset(&server, 0, sizeof(server));

	// bind and listen
	socket = aio_socket_create(tcpserver_create(NULL, PORT, 32), 1);
	r = aio_socket_accept(socket, aio_server_onaccept, &server);
	assert(0 == r);

	// client connect
	tcp = socket_tcp();
#if defined(OS_WINDOWS)
	r = socket_bind_any(tcp, 0);
#endif
	client.socket = aio_socket_create(tcp, 1);
	aio_socket_connect(client.socket, "127.0.0.1", PORT, aio_client_onconnect, &client);

	r = 1000;
	while(--r > 0)
	{
		aio_socket_process(1000);
	}

	aio_socket_destroy(socket);
	aio_socket_destroy(server.socket);
	aio_socket_destroy(client.socket);
	aio_socket_clean();
}
