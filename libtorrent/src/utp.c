#include "utp.h"
#include "utp-header.h"
#include "utp-internal.h"
#include "bsearch.h"

#define UTP_WINDOW_SIZE 50000

struct utp_t* utp_create(const uint16_t port, utp_onconnected onconnected, void* param)
{
	struct utp_t* utp;
	utp = (struct utp_t*)calloc(1, sizeof(*utp));
	if (!utp) return NULL;

	utp->onconnected = onconnected;
	utp->param = param;

	if (0 != udp_socket_create(port, &utp->udp))
	{
		utp_destroy(utp);
		utp = NULL;
	}

	return utp;
}

void utp_destroy(struct utp_t* utp)
{
	udp_socket_destroy(&utp->udp);

	if (utp->sockets)
	{
		assert(utp->capacity > 0);
		free(utp->sockets);
		utp->sockets = NULL;
	}

	free(utp);
}

int utp_input(struct utp_t* utp, const uint8_t* data, unsigned int bytes)
{
	int r;
	struct utp_header_t header;
	r = utp_header_read(data, bytes, &header);
	if (r < 0)
		return -1;
	assert(20 == r);

	if (UTP_ST_SYN == header.type)
	{
		r = utp_input_connect(utp, &header);
		if(0 == r)
			r = utp_socket_ack(utp);
	}
	else
	{
		r = utp_input_msg(utp, &header, data + r, bytes - r);
	}
	
	return r;
}

static int utp_input_connect(struct utp_t* utp, const struct utp_header_t* header)
{
	int r, pos;
	struct utp_socket_t* socket;
	socket = (struct utp_socket_t*)malloc(sizeof(*socket));
	if (!socket) return ENOMEM;

	socket->utp = utp;
	socket->headers[UTP_RECV].connection = header->connection_id + 1;
	socket->headers[UTP_RECV].timestamp = header->timestamp;
	socket->headers[UTP_RECV].clock = (uint32_t)system_clock();
	socket->headers[UTP_RECV].window = header->window_size;
	socket->headers[UTP_RECV].nr.seq = header->seq_nr;

	socket->headers[UTP_SEND].connection = header->connection_id;
	socket->headers[UTP_SEND].timestamp = 0;
	socket->headers[UTP_SEND].clock = 0;
	socket->headers[UTP_SEND].window = UTP_WINDOW_SIZE;
	socket->headers[UTP_SEND].nr.ack = rand(socket->headers[0].clock);

	if (0 == utp_find_socket(utp, header->connection_id, &pos)
		|| 0 == utp_find_socket(utp, header->connection_id + 1, &pos))
	{
		// connection id exist
		free(socket);
		return 0; // EEXIST;
	}

	utp_insert_socket(utp, socket, pos);

	r = utp->onconnected(utp->param, socket);
	if (0 != r)
	{
		free(socket);
	}

	return r;
}

static int utp_input_msg(struct utp_t* utp, const struct utp_header_t* header)
{
	int r, pos;
	struct utp_socket_t* socket;

	if (0 != utp_find_socket(utp, header->connection_id, &pos)
		|| header->connection_id != utp->sockets[pos]->headers[UTP_RECV].connection)
	{
		return 0; // ENOENT
	}

	socket = utp->sockets[pos];
	switch (header->type)
	{
	case UTP_ST_RESET:
	case UTP_ST_FIN:
		utp_remove_socket(utp, pos);
		socket->handler.ondestroy(socket->param);
		utp_socket_destroy(socket);
		break;

	case UTP_ST_STATE:
	case UTP_ST_DATA:
	default:
		break;
	}
}

/// @return 0-find, other-not found, pos insert position
static int utp_connection_compare(const uint16_t* connection, const struct utp_socket_t* socket)
{
	uint16_t v;
	v = MIN(socket->headers[UTP_RECV].connection, socket->headers[UTP_SEND].connection);
	assert(v + 1 == MAX(socket->headers[UTP_RECV].connection, socket->headers[UTP_SEND].connection));
	if (connection == v || connection == v + 1)
		return 0;	
	return connection - v;
}

static int utp_find_socket(struct utp_t* utp, uint16_t connection, int* pos)
{
	int r;
	struct utp_socket_t* socket;

	r = bsearch2(connection, utp->sockets, &socket, utp->count, sizeof(utp->sockets[0]), utp_connection_compare);
	if (0 == r)
	{
		pos = socket - utp->sockets;
	}
	return r;
}

static int utp_insert_socket(struct utp_t* utp, struct utp_socket_t* socket, int pos)
{
	if (utp->count >= utp->capacity)
	{
		void* ptr;
		ptr = realloc(utp->sockets, sizeof(utp->sockets[0]) * (utp->capacity + 100));
		if (!ptr) return ENOMEM;

		utp->sockets = (struct utp_socket_t**)ptr;
		utp->capacity += 100;
	}

	if (pos < utp->count)
		memmove(utp->sockets + pos + 1, utp->sockets + pos, (utp->count - pos) * sizeof(utp->sockets[0]));

	utp->sockets[pos] = socket;
	utp->count += 1;
	return 0;
}

static int utp_remove_socket(struct utp_t* utp, int pos)
{
	if (pos < utp->count)
		memmove(utp->sockets + pos, utp->sockets + pos + 1, (utp->count - pos - 1) * sizeof(utp->sockets[0]));

	utp->count -= 1;
	return 0;
}
