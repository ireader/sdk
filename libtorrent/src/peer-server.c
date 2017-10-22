#include "sys/sock.h"
#include "sockutil.h"
#include "aio-accept.h"
#include "aio-rwutil.h"
#include "app-log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static void* s_server;

struct peer_handshake_t
{
	struct sockaddr_storage addr;
	struct aio_socket_rw_t recv;
	aio_socket_t aio;
	uint8_t buffer[68];
};

void peer_coming(aio_socket_t aio, const struct sockaddr_storage* addr, const uint8_t flags[8], const uint8_t info_hash[20], const uint8_t peer_id[20])
{
}

static void peer_onrecv(void* param, int code, size_t bytes)
{
	struct peer_handshake_t* peer;
	peer = (struct peer_handshake_t*)param;

	if (0 == code && bytes == sizeof(peer->buffer))
	{
		// handshake message, see more @peer-message.h
		if (19 == peer->buffer[0] && 0 == memcmp("BitTorrent protocol", peer->buffer + 1, 19))
		{
			const uint8_t* flags;
			const uint8_t* info_hash;
			const uint8_t* peer_id;
			flags = peer->buffer + 20;
			info_hash = peer->buffer + 28;
			peer_id = peer->buffer + 48;
			peer_coming(peer->aio, &peer->addr, flags, info_hash, peer_id);

			// find torrent by info hash

			//peer_dispatch_peer(tor->disp, socket, addr, addrlen);
		}
	}
	else
	{
		app_log(LOG_ERROR, "[%d] first bytes must be handshake\n", code);
	}

	free(peer);
}

static void peer_handshake(socket_t socket, const struct sockaddr* addr, socklen_t addrlen)
{
	struct peer_handshake_t* peer;
	peer = (struct peer_handshake_t*)malloc(sizeof(*peer));
	if (!peer) return;

	peer->aio = aio_socket_create(socket, 1);
	memset(&peer->addr, 0, sizeof(peer->addr));
	memcpy(&peer->addr, addr, addrlen);
	if (0 != aio_socket_recv_all(&peer->recv, 3000, peer->aio, peer->buffer, sizeof(peer->buffer), peer_onrecv, peer))
		free(peer);
}

static void peer_server_onaccept(void* param, int code, socket_t socket, const struct sockaddr* addr, socklen_t addrlen)
{
	if (0 != code)
	{
		app_log(LOG_ERROR, "%s error: %d\n", __FUNCTION__, code);
		return;
	}

	peer_handshake(socket, addr, addrlen);
}

int peer_server_start(uint16_t port)
{
	socket_t s;
	s = socket_tcp_listen(NULL, port, SOMAXCONN);
	if (socket_invalid == s)
		return socket_geterror();

	s_server = aio_accept_start(s, peer_server_onaccept, NULL);
	return s_server ? 0 : socket_geterror();
}

int peer_server_stop()
{
	return aio_accept_stop(s_server, NULL, NULL);
}
