#include "aio-socket.h"
#include "sockutil.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define PORT 23456
#define ROUND 10

struct aio_client_t
{
	int cc;
	char msg[64];
	aio_socket_t socket;
	socket_bufvec_t vec[2];
	struct sockaddr_in addr; // peer
};

struct aio_server_t
{
	int cc;
	char msg[64];
	aio_socket_t socket;
	socket_bufvec_t vec[2];
	struct sockaddr_in addr; // peer
};

static const char *s_cmsg1 = "aio-udp-client-request";
static const char *s_cmsg2 = "aio-udp-client-request-v";
static const char *s_smsg1 = "aio-udp-server-reply";
static const char *s_smsg2 = "aio-udp-server-reply-v";

static void aio_socket_ondestroy(void* param)
{
	const char* msg = (const char*)param;
	printf("[AIO-UDP-RW] %s: %s\n", __FUNCTION__, msg);
}

//////////////////////////////////////////////////////////////////////////
/// Server
/// Accept -> OnAccept(Recv) -> OnRecv(Send) -> OnSend(RecvVec) -> OnRecvVec(SendVec) -> OnSendVec(Recv)
///                     ^                                                                            |
///                     |----------------------<--------------------<--------------------<-----------+
///
static void aio_server_onrecvfrom(void* param, int code, size_t bytes, const struct sockaddr* addr, socklen_t addrlen);
static void aio_server_onsendto_v(void* param, int code, size_t bytes)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes == strlen(s_smsg2));

	printf("[AIO-UDP-RW] aio-server round: %d\n", server->cc);
	if (++server->cc > ROUND) return; // exit

	memset(server->msg, 0, sizeof(server->msg));
	r = aio_socket_recvfrom(server->socket, server->msg, sizeof(server->msg), aio_server_onrecvfrom, param);
	assert(0 == r);
}

static void aio_server_onrecvfrom_v(void* param, int code, size_t bytes, const struct sockaddr* addr, socklen_t addrlen)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes == strlen(s_cmsg2));
	assert(0 == memcmp(&server->addr, addr, addrlen));
	assert(addrlen == sizeof(server->addr));
	assert(0 == strcmp(s_cmsg2, server->msg));

	socket_setbufvec(server->vec, 0, (void*)s_smsg2, strlen(s_smsg2)/2);
	socket_setbufvec(server->vec, 1, (void*)(s_smsg2+strlen(s_smsg2)/2), strlen(s_smsg2)/2);
	r = aio_socket_sendto_v(server->socket, addr, addrlen, server->vec, 2, aio_server_onsendto_v, param);
	assert(0 == r);
}

static void aio_server_onsendto(void* param, int code, size_t bytes)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes == strlen(s_smsg1));

	memset(server->msg, 0, sizeof(server->msg));
	socket_setbufvec(server->vec, 0, server->msg, strlen(s_cmsg2)/2);
	socket_setbufvec(server->vec, 1, server->msg + strlen(s_cmsg2)/2, strlen(s_cmsg2)/2);
	r = aio_socket_recvfrom_v(server->socket, server->vec, 2, aio_server_onrecvfrom_v, param);
	assert(0 == r);
}

static void aio_server_onrecvfrom(void* param, int code, size_t bytes, const struct sockaddr* addr, socklen_t addrlen)
{
	int r;
	struct aio_server_t *server = (struct aio_server_t*)param;
	assert(0 == code && bytes == strlen(s_cmsg1));
	assert(0 == memcmp(&server->addr, addr, addrlen));
	assert(addrlen == sizeof(server->addr));
	assert(0 == strcmp(s_cmsg1, server->msg));

	r = aio_socket_sendto(server->socket, addr, addrlen, s_smsg1, strlen(s_smsg1), aio_server_onsendto, param);
	assert(0 == r);
}

//////////////////////////////////////////////////////////////////////////
/// Client
/// SendTo -> OnSendTo(RecvFrom) -> OnRecvFrom(SendToVec) -> OnSendToVec(RecvFromVec) -> OnRecvFromVec(SendTo)
///  ^                                                                                                    |
///  |----------------------<--------------------<--------------------<-----------------------------------+
///
static void aio_client_onsendto(void* param, int code, size_t bytes);
static void aio_client_onrecvfrom_v(void* param, int code, size_t bytes, const struct sockaddr* addr, socklen_t addrlen)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes == strlen(s_smsg2));
	assert(0 == memcmp(&client->addr, addr, addrlen));
	assert(addrlen == sizeof(client->addr));
	assert(0 == strcmp(s_smsg2, client->msg));

	printf("[AIO-UDP-RW] aio-client round: %d\n", client->cc);
	if (++client->cc > ROUND) return; // exit

	r = aio_socket_sendto(client->socket, addr, addrlen, s_cmsg1, strlen(s_cmsg1), aio_client_onsendto, param);
	assert(0 == r);
}

static void aio_client_onsendto_v(void* param, int code, size_t bytes)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes == strlen(s_cmsg2));

	memset(client->msg, 0, sizeof(client->msg));
	socket_setbufvec(client->vec, 0, client->msg, strlen(s_smsg2)/2);
	socket_setbufvec(client->vec, 1, client->msg + strlen(s_smsg2)/2, strlen(s_smsg2)/2);
	r = aio_socket_recvfrom_v(client->socket, client->vec, 2, aio_client_onrecvfrom_v, param);
	assert(0 == r);
}

static void aio_client_onrecvfrom(void* param, int code, size_t bytes, const struct sockaddr* addr, socklen_t addrlen)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes == strlen(s_smsg1));
	assert(0 == memcmp(&client->addr, addr, addrlen));
	assert(addrlen == sizeof(client->addr));
	assert(0 == strcmp(s_smsg1, client->msg));

	socket_setbufvec(client->vec, 0, (void*)s_cmsg2, strlen(s_cmsg2) / 2);
	socket_setbufvec(client->vec, 1, (void*)(s_cmsg2 + strlen(s_cmsg2) / 2), strlen(s_cmsg2) / 2);
	r = aio_socket_sendto_v(client->socket, addr, addrlen, client->vec, 2, aio_client_onsendto_v, param);
	assert(0 == r);
}

static void aio_client_onsendto(void* param, int code, size_t bytes)
{
	int r;
	struct aio_client_t *client = (struct aio_client_t*)param;
	assert(0 == code && bytes == strlen(s_cmsg1));

	memset(client->msg, 0, sizeof(client->msg));
	r = aio_socket_recvfrom(client->socket, client->msg, strlen(s_smsg1), aio_client_onrecvfrom, param);
	assert(0 == r);
}

void aio_socket_test3(void)
{
	struct aio_client_t client;
	struct aio_server_t server;

	aio_socket_init(1);

	memset(&client, 0, sizeof(client));
	memset(&server, 0, sizeof(server));
	socket_addr_from_ipv4(&client.addr, "127.0.0.1", PORT);
	socket_addr_from_ipv4(&server.addr, "127.0.0.1", PORT + 1);
	server.socket = aio_socket_create(socket_udp_bind_ipv4("127.0.0.1", PORT), 1);
	client.socket = aio_socket_create(socket_udp_bind_ipv4("127.0.0.1", PORT+1), 1);

	assert(0 == aio_socket_recvfrom(server.socket, server.msg, strlen(s_cmsg1), aio_server_onrecvfrom, &server));
	assert(0 == aio_socket_sendto(client.socket, (struct sockaddr*)&client.addr, sizeof(client.addr), s_cmsg1, strlen(s_cmsg1), aio_client_onsendto, &client));

	while (aio_socket_process(1000) > 0); // handle io
	aio_socket_destroy(server.socket, aio_socket_ondestroy, "[AIO-TCP-RW] sever destroy");
	aio_socket_destroy(client.socket, aio_socket_ondestroy, "[AIO-TCP-RW] client destroy");
	while (aio_socket_process(1000) > 0); // handle destroy

	aio_socket_clean();
}
