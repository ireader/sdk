#include "aio-socket.h"
#include "aio-rwutil.h"
#include "sockutil.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define PORT 23456
#define ROUND 10
#define TIMEOUT_RECV 2000
#define TIMEOUT_SEND 5000

struct aio_client_t
{
	int cc;
	char msg[64];
	aio_socket_t socket;
	socket_bufvec_t vec[2];
	struct aio_socket_rw_t rw;
};

struct aio_server_t
{
	int cc;
	char msg[64];
	aio_socket_t socket;
	socket_bufvec_t vec[2];
	struct aio_socket_rw_t rw;
};

static const char *s_cmsg1 = "aio-tcp-client-request";
static const char *s_cmsg2 = "aio-tcp-client-request-v";
static const char *s_smsg1 = "aio-tcp-server-reply";
static const char *s_smsg2 = "aio-tcp-server-reply-v";

static void aio_socket_ondestroy(void* param)
{
	const char* msg = (const char*)param;
	printf("[AIO-TCP-RW] %s: %s\n", __FUNCTION__, msg);
}

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

	printf("[AIO-TCP-RW] aio-server round: %d\n", server->cc);
	if (++server->cc > ROUND) return; // exit

	memset(server->msg, 0, sizeof(server->msg));
	r = aio_socket_recv_all(&server->rw, TIMEOUT_RECV, server->socket, server->msg, strlen(s_cmsg1), aio_server_onrecv, param);
	assert(0 == r);
}

static void aio_server_onrecv_v(void* param, int code, size_t bytes)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes == strlen(s_cmsg2));
	assert(0 == strcmp(s_cmsg2, server->msg));
	socket_setbufvec(server->vec, 0, (void*)s_smsg2, strlen(s_smsg2)/2);
	socket_setbufvec(server->vec, 1, (void*)(s_smsg2+strlen(s_smsg2)/2), strlen(s_smsg2)/2);
	r = aio_socket_send_v_all(&server->rw, TIMEOUT_SEND, server->socket, server->vec, 2, aio_server_onsend_v, param);
	assert(0 == r);
}

static void aio_server_onsend(void* param, int code, size_t bytes)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes == strlen(s_smsg1));

	memset(server->msg, 0, sizeof(server->msg));
	socket_setbufvec(server->vec, 0, server->msg, strlen(s_cmsg2)/2);
	socket_setbufvec(server->vec, 1, server->msg+ strlen(s_cmsg2)/2, strlen(s_cmsg2)/2);
	r = aio_socket_recv_v_all(&server->rw, TIMEOUT_RECV, server->socket, server->vec, 2, aio_server_onrecv_v, param);
	assert(0 == r);
}

static void aio_server_onrecv(void* param, int code, size_t bytes)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes == strlen(s_cmsg1));
	assert(0 == strcmp(s_cmsg1, server->msg));
	r = aio_socket_send_all(&server->rw, TIMEOUT_SEND, server->socket, s_smsg1, strlen(s_smsg1), aio_server_onsend, param);
	assert(0 == r);
}

static void aio_server_onaccept(void* param, int code, socket_t socket, const struct sockaddr* addr, socklen_t addrlen)
{
	int r;
	unsigned short port;
	char ip[SOCKET_ADDRLEN];
	struct aio_server_t *server = (struct aio_server_t*)param;

	assert(0 == code);
	socket_addr_to(addr, addrlen, ip, &port);
	printf("[AIO-TCP-RW] aio_server_onaccept: %s:%hu\n", ip, port);

	memset(server->msg, 0, sizeof(server->msg));
	server->socket = aio_socket_create(socket, 1);
	r = aio_socket_recv_all(&server->rw, TIMEOUT_RECV, server->socket, server->msg, strlen(s_cmsg1), aio_server_onrecv, param);
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
	assert(0 == code && bytes == strlen(s_smsg2));
	assert(0 == strcmp(s_smsg2, client->msg));

	printf("[AIO-TCP-RW] aio-client round: %d\n", client->cc);
	if (++client->cc > ROUND) return; // exit

	r = aio_socket_send_all(&client->rw, TIMEOUT_SEND, client->socket, s_cmsg1, strlen(s_cmsg1), aio_client_onsend, param);
	assert(0 == r);
}

static void aio_client_onsend_v(void* param, int code, size_t bytes)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes == strlen(s_cmsg2));
	memset(client->msg, 0, sizeof(client->msg));
	socket_setbufvec(client->vec, 0, client->msg, strlen(s_smsg2)/2);
	socket_setbufvec(client->vec, 1, client->msg + strlen(s_smsg2)/2, strlen(s_smsg2)/2);
	r = aio_socket_recv_v_all(&client->rw, TIMEOUT_RECV, client->socket, client->vec, 2, aio_client_onrecv_v, param);
	assert(0 == r);
}

static void aio_client_onrecv(void* param, int code, size_t bytes)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes == strlen(s_smsg1));
	assert(0 == strcmp(s_smsg1, client->msg));
	socket_setbufvec(client->vec, 0, (void*)s_cmsg2, strlen(s_cmsg2)/2);
	socket_setbufvec(client->vec, 1, (void*)(s_cmsg2+strlen(s_cmsg2)/2), strlen(s_cmsg2)/2);
	r = aio_socket_send_v_all(&client->rw, TIMEOUT_SEND, client->socket, client->vec, 2, aio_client_onsend_v, param);
	assert(0 == r);
}

static void aio_client_onsend(void* param, int code, size_t bytes)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes == strlen(s_cmsg1));
	memset(client->msg, 0, sizeof(client->msg));
	r = aio_socket_recv_all(&client->rw, TIMEOUT_RECV, client->socket, client->msg, strlen(s_smsg1), aio_client_onrecv, param);
	assert(0 == r);
}

static void aio_client_onconnect(void* param, int code)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code);
	printf("[AIO-TCP-RW] %s\n", __FUNCTION__);
	r = aio_socket_send_all(&client->rw, TIMEOUT_SEND, client->socket, s_cmsg1, strlen(s_cmsg1), aio_client_onsend, param);
	assert(0 == r);
}

void aio_socket_test2(void)
{
	int r;
	socket_t tcp;
	aio_socket_t socket;
	struct aio_client_t client;
	struct aio_server_t server;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	aio_socket_init(1);

	memset(&client, 0, sizeof(client));
	memset(&server, 0, sizeof(server));

	socket_addr_from_ipv4(&addr, "127.0.0.1", PORT);

	// bind and listen
	socket = aio_socket_create(socket_tcp_listen_ipv4(NULL, PORT, 32), 1);
	r = aio_socket_accept(socket, aio_server_onaccept, &server);
	assert(0 == r);

	// client connect
	tcp = socket_tcp();
#if defined(OS_WINDOWS)
	r = socket_bind_any(tcp, 0);
#endif
	client.socket = aio_socket_create(tcp, 1);
	aio_socket_connect(client.socket, (struct sockaddr*)&addr, addrlen, aio_client_onconnect, &client);

	while (aio_socket_process(1000) > 0); // handle io
	aio_socket_destroy(socket, aio_socket_ondestroy, "[AIO-TCP-RW] listen socket destroy");
	aio_socket_destroy(server.socket, aio_socket_ondestroy, "[AIO-TCP-RW] sever destroy");
	aio_socket_destroy(client.socket, aio_socket_ondestroy, "[AIO-TCP-RW] client destroy");
	while (aio_socket_process(1000) > 0); // handle destroy

	aio_socket_clean();
}
