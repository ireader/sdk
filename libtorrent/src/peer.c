#include "peer.h"
#include "peer-parser.h"
#include "peer-message.h"
#include "aio-tcp-transport.h"
#include "sys/locker.h"
#include "sys/system.h"
#include "byte-order.h"
#include "bitmap.h"
#include "list.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct peer_msg_t
{
	struct list_head link;
	int type;
	uint8_t* msg;
	uint32_t bytes;
};

struct peer_info_t
{
	uint8_t flags[8];
	uint8_t id[20];

	unsigned int choke : 1; // don't send data to peer
	unsigned int interested : 1; // need something from peer
	unsigned int peer_choke : 1; // peer block me
	unsigned int peer_interested : 1; // peer request data

	uint8_t* bitfield;
	uint32_t bits;

	struct sockaddr_storage addr;
};

struct peer_t
{
	struct peer_info_t base;
	struct peer_parser_t parser;
	struct peer_handler_t handler;
	void* param;

	locker_t locker;
	aio_tcp_transport_t* aio;
	uint8_t rbuffer[N_PIECE_SLICE + 128];
	struct peer_msg_t* msg; // sending msg
	struct list_head messages;

	int recv_timeout; // 0-ok, 1-keepalive, >=2-timeout
};

static int peer_onbitfield(struct peer_t* peer, uint8_t* bitfield, uint32_t count);
static int peer_oncancel(void* param, uint32_t piece, uint32_t begin, uint32_t length);
static int peer_handler(void* param, struct peer_parser_t* parser);
static int peer_dispatch(peer_t* peer);
static int peer_keepalive(peer_t* peer);

static void peer_aio_ondestroy(void* param)
{
	struct peer_msg_t* msg;
	struct list_head *pos, *next;
	peer_t* peer = (peer_t*)param;

	if (peer->parser.buffer)
	{
		assert(peer->parser.capacity > 0);
		free(peer->parser.buffer);
		peer->parser.buffer = NULL;
	}

	list_for_each_safe(pos, next, &peer->messages)
	{
		msg = list_entry(pos, struct peer_msg_t, link);
		free(msg);
	}

	if (peer->msg)
	{
		free(peer->msg);
		peer->msg = NULL;
	}

	if (peer->base.bitfield)
	{
		free(peer->base.bitfield);
		peer->base.bitfield = NULL;
	}

	locker_destroy(&peer->locker);
	free(peer);
}

static void peer_aio_onrecv(void* param, int code, size_t bytes)
{
	peer_t* peer = (peer_t*)param;
	if (0 == code)
	{
		if (0 == bytes)
		{
			code = ECONNRESET;
		}
		else
		{
			peer->recv_timeout = 0;
			code = peer_input(&peer->parser, peer->rbuffer, bytes, peer_handler, peer);
			if (0 == code)
				code = aio_tcp_transport_recv(peer->aio, peer->rbuffer, sizeof(peer->rbuffer));
		}
	}
	else if (ETIMEDOUT == code)
	{
		if (1 == ++peer->recv_timeout)
		{
			// send keep alive
			peer_keepalive(peer);

			code = aio_tcp_transport_recv(peer->aio, peer->rbuffer, sizeof(peer->rbuffer));
		}
	}

	if (0 != code)
	{
		peer->handler.error(peer->param, code);
	}
}

static void peer_aio_onsend(void* param, int code, size_t bytes)
{
	peer_t* peer = (peer_t*)param;

	if (0 == code)
	{
		locker_lock(&peer->locker);

		// finish packet
		assert(peer->msg && bytes == peer->msg->bytes);
		free(peer->msg);
		peer->msg = NULL;

		// send next packet
		code = peer_dispatch(peer);

		locker_unlock(&peer->locker);
	}

	if (0 != code)
	{
		assert(0);
		peer->handler.error(peer->param, code);
	}
}

peer_t* peer_create(aio_socket_t aio, const struct sockaddr_storage* addr, struct peer_handler_t* handler, void* param)
{
	peer_t* peer;
	struct aio_tcp_transport_handler_t aiohandler;
	peer = (peer_t*)calloc(1, sizeof(*peer));
	if (!peer) return NULL;

	memset(&aiohandler, 0, sizeof(aiohandler));
	aiohandler.ondestroy = peer_aio_ondestroy;
	aiohandler.onrecv = peer_aio_onrecv;
	aiohandler.onsend = peer_aio_onsend;
	peer->aio = aio_tcp_transport_create2(aio, &aiohandler, peer);

	locker_create(&peer->locker);
	LIST_INIT_HEAD(&peer->messages);
	memcpy(&peer->handler, handler, sizeof(peer->handler));
	peer->param = param;
	peer->recv_timeout = 0;

	memcpy(&peer->base.addr, addr, sizeof(peer->base.addr));
	peer->base.choke = 1;
	peer->base.interested = 0;
	peer->base.peer_choke = 1;
	peer->base.peer_interested = 0;
	//memset(&peer->parser, 0, sizeof(peer->parser));

	// Keepalives are generally sent once every two minutes
	// set recv timeout 90s
	aio_tcp_transport_set_timeout(peer->aio, 90 * 1000, 5 * 1000);

	if (0 != aio_tcp_transport_recv(peer->aio, peer->rbuffer, sizeof(peer->rbuffer)))
	{
		peer_destroy(peer);
		return NULL;
	}
	return peer;
}

void peer_destroy(peer_t* peer)
{
	aio_tcp_transport_destroy(peer->aio);
}

int peer_choke(peer_t* peer, int choke)
{
	int r;
	struct peer_msg_t* msg;
	msg = malloc(sizeof(*msg) + 5);
	msg->type = choke ? BT_CHOKE : BT_UNCHOKE;
	msg->msg = (uint8_t*)(msg + 1);
	msg->bytes = 5;
	if (choke)
		peer_choke_write(msg->msg);
	else
		peer_unchoke_write(msg->msg);

	locker_lock(&peer->locker);
	list_insert_after(&msg->link, peer->messages.prev);
	r = peer_dispatch(peer);
	locker_unlock(&peer->locker);
	return r;
}

int peer_interested(peer_t* peer, int interested)
{
	int r;
	struct peer_msg_t* msg;
	msg = malloc(sizeof(*msg) + 5);
	msg->type = interested ? BT_INTERESTED : BT_NONINTERESTED;
	msg->msg = (uint8_t*)(msg + 1);
	msg->bytes = 5;
	if (interested)
		peer_interested_write(msg->msg);
	else
		peer_noninterested_write(msg->msg);

	locker_lock(&peer->locker);
	list_insert_after(&msg->link, peer->messages.prev);
	r = peer_dispatch(peer);
	locker_unlock(&peer->locker);
	return r;
}

int peer_have(peer_t* peer, uint32_t piece)
{
	int r;
	struct peer_msg_t* msg;
	msg = malloc(sizeof(*msg) + 9);
	msg->type = BT_HAVE;
	msg->msg = (uint8_t*)(msg + 1);
	msg->bytes = 9;
	peer_have_write(msg->msg, piece);

	locker_lock(&peer->locker);
	list_insert_after(&msg->link, peer->messages.prev);
	r = peer_dispatch(peer);
	locker_unlock(&peer->locker);
	return r;
}

int peer_recv(peer_t* peer, uint32_t piece, uint32_t begin, uint32_t length)
{
	int r;
	uint8_t* p;
	uint32_t i, n;
	struct peer_msg_t* msg;
	assert(0 == length % N_PIECE_SLICE);
	n = (length + N_PIECE_SLICE - 1) / N_PIECE_SLICE;
	msg = (struct peer_msg_t*)malloc(sizeof(*msg) + 17 * n);
	msg->type = BT_REQUEST;
	msg->bytes = 17 * n;
	msg->msg = (uint8_t*)(msg + 1);
	p = msg->msg;

	for (i = 0; i < n; i++)
	{
		r = length > N_PIECE_SLICE ? N_PIECE_SLICE : length;
		r = peer_request_write(p, piece, begin, r);
		assert(r == 17);
		p += r;
		begin += N_PIECE_SLICE;
		length -= N_PIECE_SLICE;
	}

	locker_lock(&peer->locker);
	list_insert_after(&msg->link, peer->messages.prev);
	r = peer_dispatch(peer);
	locker_unlock(&peer->locker);
	return r;
}

int peer_send_slices(peer_t* peer, uint32_t piece, uint32_t begin, uint32_t length, const uint8_t* data)
{
	int r;
	struct peer_msg_t* msg;
	assert(0 == length % N_PIECE_SLICE);
	while (length > 0)
	{
		msg = malloc(sizeof(*msg) + 17 + N_PIECE_SLICE);
		if (!msg) return ENOMEM;

		r = length > N_PIECE_SLICE ? N_PIECE_SLICE : length;

		msg->type = BT_PIECE;
		msg->msg = (uint8_t*)(msg + 1);
		msg->bytes = peer_piece_write(msg->msg, 17 + N_PIECE_SLICE, piece, begin, r, data);
		data += r;
		length -= r;

		locker_lock(&peer->locker);
		list_insert_after(&msg->link, peer->messages.prev);
		locker_unlock(&peer->locker);
	}

	locker_lock(&peer->locker);
	r = peer_dispatch(peer);
	locker_unlock(&peer->locker);
	return r;
}

int peer_send_meta(peer_t* peer, const void* meta, uint32_t bytes)
{
	int r;
	struct peer_msg_t* msg;
	msg = malloc(sizeof(*msg) + bytes);
	msg->type = BT_KEEPALIVE;
	msg->msg = (uint8_t*)(msg + 1);
	msg->bytes = bytes;
	memcpy(msg->msg, meta, bytes);

	locker_lock(&peer->locker);
	list_insert_after(&msg->link, peer->messages.prev);
	r = peer_dispatch(peer);
	locker_unlock(&peer->locker);
	return r;
}

int peer_empty(const peer_t* peer)
{
	return list_empty(&peer->messages) ? 1 : 0;
}

int peer_handshake(peer_t* peer, const uint8_t info_hash[20], const uint8_t id[20])
{
	int r;
	struct peer_msg_t* msg;
	msg = malloc(sizeof(*msg) + 68);
	msg->type = BT_HANDSHAKE;
	msg->msg = (uint8_t*)(msg + 1);
	msg->bytes = 68;
	peer_handshake_write(msg->msg, info_hash, id);

	locker_lock(&peer->locker);
	list_insert_after(&msg->link, peer->messages.prev);
	r = peer_dispatch(peer);
	locker_unlock(&peer->locker);
	return r;
}

int peer_extended(peer_t* peer, uint16_t port, const char* version)
{
	int r;
	struct peer_msg_t* msg;
	msg = malloc(sizeof(*msg) + 128);
	msg->type = BT_EXTENDED;
	msg->msg = (uint8_t*)(msg + 1);
	msg->bytes = peer_extended_write(msg->msg, 128, port, version);

	locker_lock(&peer->locker);
	list_insert_after(&msg->link, peer->messages.prev);
	r = peer_dispatch(peer);
	locker_unlock(&peer->locker);
	return r;
}

int peer_bitfield(peer_t* peer, const uint8_t* bitfield, uint32_t bits)
{
	int r;
	struct peer_msg_t* msg;
	
	r = (bits + 7) / 8;
	msg = malloc(sizeof(*msg) + 5 + r);
	msg->type = BT_BITFIELD;
	msg->msg = (uint8_t*)(msg + 1);
	msg->bytes = peer_bitfield_write(msg->msg, 5 + r, bitfield, r);

	locker_lock(&peer->locker);
	list_insert_after(&msg->link, peer->messages.prev);
	r = peer_dispatch(peer);
	locker_unlock(&peer->locker);
	return r;
}

// BEP 3 -> peer protocol
// Messages of length zero are keepalives, and ignored. 
// Keepalives are generally sent once every two minutes, 
// but note that timeouts can be done much more quickly when data is expected.
static int peer_keepalive(peer_t* peer)
{
	int r;
	struct peer_msg_t* msg;
	msg = malloc(sizeof(*msg) + 4);
	msg->type = BT_KEEPALIVE;
	msg->msg = (uint8_t*)(msg + 1);
	msg->bytes = 4;
	nbo_w32(msg->msg, 0);

	locker_lock(&peer->locker);
	list_insert_after(&msg->link, peer->messages.prev);
	r = peer_dispatch(peer);
	locker_unlock(&peer->locker);
	return r;
}

static int peer_dispatch(peer_t* peer)
{
	int r = 0;
	struct peer_msg_t* msg;
	if (!peer->msg && !list_empty(&peer->messages))
	{
		msg = list_entry(peer->messages.next, struct peer_msg_t, link);
		r = aio_tcp_transport_send(peer->aio, msg->msg, msg->bytes);
		if (0 == r)
		{
			list_remove(&msg->link);
			peer->msg = msg; // sending status
		}
	}
	return r;
}

static int peer_handler(void* param, struct peer_parser_t* parser)
{
	int r;
	uint16_t port;
	uint8_t* bitfield, *slice;
	uint8_t info_hash[20];
	uint32_t piece, begin, length;
	peer_t* peer = (peer_t*)param;

	switch (parser->type)
	{
	case BT_KEEPALIVE:
		return 0; // do nothing

	case BT_HANDSHAKE:
		r = peer_handshake_read(parser->buffer, parser->len, peer->base.flags, info_hash, peer->base.id);
		return r < 0 ? r : peer->handler.handshake(peer->param, peer->base.flags, info_hash, peer->base.id);

	case BT_CHOKE:
	case BT_UNCHOKE:
		peer->base.peer_choke = BT_CHOKE == parser->type ? 1 : 0;
		return peer->handler.choke(peer->param, peer->base.peer_choke);

	case BT_INTERESTED:
	case BT_NONINTERESTED:
		peer->base.peer_interested = BT_INTERESTED == parser->type ? 1 : 0;
		return peer->handler.interested(peer->param, peer->base.peer_interested);

	case BT_HAVE:
		r = peer_have_read(parser->buffer, parser->len, &piece);
		if (r < 0 || piece >= peer->base.bits)
			return -1;
		bitmap_set(peer->base.bitfield, piece, 1);
		return peer->handler.have(peer->param, piece);

	case BT_BITFIELD:
		r = peer_bitfield_read(parser->buffer, parser->len, &bitfield, &length);
		return r < 0 ? r : peer_onbitfield(peer, bitfield, length);

	case BT_REQUEST:
		r = peer_request_read(parser->buffer, parser->len, &piece, &begin, &length);
		return r < 0 ? r : peer->handler.request(peer->param, piece, begin, length);

	case BT_PIECE:
		r = peer_piece_read(parser->buffer, parser->len, &piece, &begin, &length, &slice);
		return r < 0 ? r : peer->handler.piece(peer->param, piece, begin, length, slice);

	case BT_CANCEL:
		r = peer_cancel_read(parser->buffer, parser->len, &piece, &begin, &length);
		return r < 0 ? r : peer_oncancel(peer, piece, begin, length);

	case BT_EXTENDED:
		r = peer_extended_read(parser->buffer, parser->len);
		return r < 0 ? r : 0;

	case BT_PORT:
		r = peer_port_read(parser->buffer, parser->len, &port);
		return r < 0 ? r : 0;

	case BT_REJECT:
	case BT_HASH_REQUEST:
	case BT_HASHES:
	case BT_HASH_REJECT:
		assert(0); // TODO
		break;

	default:
		assert(0);
		break;
	}
	return 0;
}

static int peer_onbitfield(struct peer_t* peer, uint8_t* bitfield, uint32_t count)
{
	peer->base.bits = count * 8;
	if (peer->base.bitfield)
		free(peer->base.bitfield);
	peer->base.bitfield = bitfield;
	
	return peer->handler.bitfield(peer->param, peer->base.bitfield, peer->base.bits);
}

static int peer_oncancel(void* param, uint32_t piece, uint32_t begin, uint32_t length)
{
	struct list_head *pos, *n;
	struct peer_msg_t* msg;
	struct peer_t* peer;
	peer = (struct peer_t*)param;

	// cancel request
	locker_lock(&peer->locker);
	list_for_each_safe(pos, n, &peer->messages)
	{
		msg = list_entry(pos, struct peer_msg_t, link);
		if (BT_PIECE == msg->type)
		{
			uint32_t p, b;
			nbo_r32(msg->msg, &p);
			nbo_r32(msg->msg + 4, &b);
			if (p == piece && begin <= b && b <= begin + length)
			{
				list_remove(pos);
				free(msg);
			}
		}
	}
	locker_unlock(&peer->locker);
	return 0;
}
