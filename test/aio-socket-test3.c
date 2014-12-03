#include "cstringext.h"
#include "udpsocket.h"
#include "aio-socket.h"
#include <errno.h>

#define PORT 23456

struct aio_client_t
{
	aio_socket_t socket;
	int cc;
	int len;
	char msg[64];
	socket_bufvec_t vec[2];
};

struct aio_server_t
{
	aio_socket_t socket;
	int cc;
	int len;
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
static void aio_server_onrecvfrom(void* param, int code, size_t bytes, const char* ip, int port);
static void aio_server_onsendto_v(void* param, int code, size_t bytes)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes == strlen(s_smsg2));

	printf("aio-server round: %d\n", ++server->cc);

	server->len = 0;
	memset(server->msg, 0, sizeof(server->msg));
	r = aio_socket_recvfrom(server->socket, server->msg, strlen(s_cmsg1), aio_server_onrecvfrom, param);
	assert(0 == r);
}

static void aio_server_onrecvfrom_v(void* param, int code, size_t bytes, const char* ip, int port)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes <= strlen(s_cmsg2));
	assert(port == PORT+1);

	server->len += bytes;
	if(bytes < strlen(s_cmsg2))
	{
		if(server->len >= sizeof(server->msg)/2)
		{
			socket_setbufvec(server->vec, 0, server->msg+server->len, sizeof(server->msg)-server->len);
			r = aio_socket_recvfrom_v(server->socket, server->vec, 1, aio_server_onrecvfrom_v, param);
			assert(0 == r);
		}
		else
		{
			socket_setbufvec(server->vec, 0, server->msg+server->len, sizeof(server->msg)/2-server->len);
			socket_setbufvec(server->vec, 1, server->msg+sizeof(server->msg)/2, sizeof(server->msg)/2);
			r = aio_socket_recvfrom_v(server->socket, server->vec, 2, aio_server_onrecvfrom_v, param);
			assert(0 == r);
		}
	}
	else
	{
		assert(0 == strcmp(s_cmsg2, server->msg));
		socket_setbufvec(server->vec, 0, s_smsg2, strlen(s_smsg2)/2);
		socket_setbufvec(server->vec, 1, s_smsg2+strlen(s_smsg2)/2, strlen(s_smsg2)/2);
		r = aio_socket_sendto_v(server->socket, "127.0.0.1", PORT+1, server->vec, 2, aio_server_onsendto_v, param);
		assert(0 == r);
	}
}

static void aio_server_onsendto(void* param, int code, size_t bytes)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes == strlen(s_smsg1));

	server->len = 0;
	memset(server->msg, 0, sizeof(server->msg));
	socket_setbufvec(server->vec, 0, server->msg, sizeof(server->msg)/2);
	socket_setbufvec(server->vec, 1, server->msg+sizeof(server->msg)/2, sizeof(server->msg)/2);
	r = aio_socket_recvfrom_v(server->socket, server->vec, 2, aio_server_onrecvfrom_v, param);
	assert(0 == r);
}

static void aio_server_onrecvfrom(void* param, int code, size_t bytes, const char* ip, int port)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes <= strlen(s_cmsg1));
	assert(port == PORT+1);

	server->len += bytes;
	if(bytes < strlen(s_cmsg1))
	{
		r = aio_socket_recvfrom(server->socket, server->msg+server->len, strlen(s_cmsg1)-server->len, aio_server_onrecvfrom, param);
		assert(0 == r);
	}
	else
	{
		assert(0 == strcmp(s_cmsg1, server->msg));

		r = aio_socket_sendto(server->socket, "127.0.0.1", PORT+1, s_smsg1, strlen(s_smsg1), aio_server_onsendto, param);
		assert(0 == r);
	}
}


//////////////////////////////////////////////////////////////////////////
/// Client
/// SendTo -> OnSendTo(RecvFrom) -> OnRecvFrom(SendToVec) -> OnSendToVec(RecvFromVec) -> OnRecvFromVec(SendTo)
///  ^                                                                                                    |
///  |----------------------<--------------------<--------------------<-----------------------------------+
///
static void aio_client_onsendto(void* param, int code, size_t bytes);
static void aio_client_onrecvfrom_v(void* param, int code, size_t bytes, const char* ip, int port)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes <= strlen(s_smsg2));
	assert(port == PORT);

	client->len += bytes;
	if(bytes < strlen(s_smsg2))
	{
		if(client->len >= sizeof(client->msg)/2)
		{
			socket_setbufvec(client->vec, 0, client->msg+client->len, sizeof(client->msg)-client->len);
			r = aio_socket_recvfrom_v(client->socket, client->vec, 1, aio_client_onrecvfrom_v, param);
			assert(0 == r);
		}
		else
		{
			socket_setbufvec(client->vec, 0, client->msg+client->len, sizeof(client->msg)/2-client->len);
			socket_setbufvec(client->vec, 1, client->msg+sizeof(client->msg)/2, sizeof(client->msg)/2);
			r = aio_socket_recvfrom_v(client->socket, client->vec, 2, aio_client_onrecvfrom_v, param);
			assert(0 == r);
		}
	}
	else
	{
		assert(0 == strcmp(s_smsg2, client->msg));

		printf("aio-client round: %d\n", ++client->cc);
		r = aio_socket_sendto(client->socket, "127.0.0.1", PORT, s_cmsg1, strlen(s_cmsg1), aio_client_onsendto, param);
		assert(0 == r);
	}
}

static void aio_client_onsendto_v(void* param, int code, size_t bytes)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes <= strlen(s_cmsg2));

	client->len = 0;
	memset(client->msg, 0, sizeof(client->msg));

	socket_setbufvec(client->vec, 0, client->msg, sizeof(client->msg)/2);
	socket_setbufvec(client->vec, 1, client->msg+sizeof(client->msg)/2, sizeof(client->msg)/2);
	r = aio_socket_recvfrom_v(client->socket, client->vec, 2, aio_client_onrecvfrom_v, param);
	assert(0 == r);
}

static void aio_client_onrecvfrom(void* param, int code, size_t bytes, const char* ip, int port)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes <= strlen(s_smsg1));
	assert(port == PORT);

	client->len += bytes;
	if(bytes < strlen(s_smsg1))
	{
		r = aio_socket_recvfrom(client->socket, client->msg+client->len, strlen(s_smsg1)-client->len, aio_client_onrecvfrom, param);
		assert(0 == r);
	}
	else
	{
		assert(0 == strcmp(s_smsg1, client->msg));

		socket_setbufvec(client->vec, 0, s_cmsg2, strlen(s_cmsg2)/2);
		socket_setbufvec(client->vec, 1, s_cmsg2+strlen(s_cmsg2)/2, strlen(s_cmsg2)/2);
		r = aio_socket_sendto_v(client->socket, "127.0.0.1", PORT, client->vec, 2, aio_client_onsendto_v, param);
		assert(0 == r);
	}
}

static void aio_client_onsendto(void* param, int code, size_t bytes)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes == strlen(s_cmsg1));

	client->len = 0;
	memset(client->msg, 0, sizeof(client->msg));
	r = aio_socket_recvfrom(client->socket, client->msg, strlen(s_smsg1), aio_client_onrecvfrom, param);
	assert(0 == r);
}

void aio_socket_test3(void)
{
	int r;
	struct aio_client_t client;
	struct aio_server_t server;

	aio_socket_init(1);

	memset(&client, 0, sizeof(client));
	memset(&server, 0, sizeof(server));

	server.socket = aio_socket_create(udpsocket_create(NULL, PORT), 1);
	client.socket = aio_socket_create(udpsocket_create(NULL, PORT+1), 1);

	printf("aio-server round: %d\n", ++server.cc);
	r = aio_socket_recvfrom(server.socket, server.msg, strlen(s_cmsg1), aio_server_onrecvfrom, &server);
	assert(0 == r);

	printf("aio-client round: %d\n", ++client.cc);
	r = aio_socket_sendto(client.socket, "127.0.0.1", PORT, s_cmsg1, strlen(s_cmsg1), aio_client_onsendto, &client);
	assert(0 == r);

	r = 1000;
	while(--r > 0)
	{
		aio_socket_process(1000);
	}

	aio_socket_destroy(server.socket);
	aio_socket_destroy(client.socket);
	aio_socket_clean();
}
