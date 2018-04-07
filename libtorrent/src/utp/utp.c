#include "utp-internal.h"
#include "sys/system.h"
#include "utp.h"
#include "utp-header.h"
#include "udp-buffer.h"
#include "bsearch.h"

#define UTP_WINDOW_SIZE 50000
#define UTP_SOCKET_BUFFER (8*1024*1024)

static int utp_find_socket(struct utp_t* utp, uint16_t connection, const struct sockaddr_storage* addr);
static struct utp_socket_t* utp_input_connect(struct utp_t* utp, const struct utp_header_t* header, const struct sockaddr_storage* addr);

struct utp_t* utp_create(const uint16_t port, struct utp_hander_t* handler, void* param)
{
    struct utp_t* utp;
    utp = (struct utp_t*)calloc(1, sizeof(*utp));
    if (!utp) return NULL;

    utp->ptr = udp_buffer_create(UTP_SOCKET_BUFFER);
    memcpy(&utp->handler, handler, sizeof(utp->handler));
    utp->param = param;

    if (!utp->ptr || 0 != udp_socket_create(port, &utp->udp))
    {
        utp_destroy(utp);
        utp = NULL;
    }

    darray_init(&utp->sockets, sizeof(struct utp_socket_t*), 8);
    return utp;
}

void utp_destroy(struct utp_t* utp)
{
    int i;
	udp_socket_destroy(&utp->udp);

    if (utp->ptr)
    {
        udp_buffer_destroy(utp->ptr);
        utp->ptr = NULL;
    }

    for (i = 0; i < darray_count(&utp->sockets); i++)
        free(darray_get(&utp->sockets, i));
    darray_free(&utp->sockets);

	free(utp);
}

int utp_input(struct utp_t* utp, const uint8_t* data, unsigned int bytes, const struct sockaddr_storage* addr)
{
	int r, pos;
	struct utp_header_t header;
	struct utp_socket_t* socket;

	r = utp_header_read(data, bytes, &header);
	if (r < 0)
		return -1;
	assert(20 == r);

	pos = utp_find_socket(utp, header.connection_id + (UTP_ST_SYN == header.type ? 1 : 0), addr);
	
	switch (header.type)
	{
	case UTP_ST_SYN:
		if (-1 != pos)
		{
			// connection exist, ignore
			return 0; // EEXIST
		}

		socket = utp_input_connect(utp, &header, addr);
		if (socket)
			r = utp_socket_ack(socket); // auto ack connection
		else
			r = ENOMEM;
		return r;

	case UTP_ST_RESET:
	case UTP_ST_FIN:
		if (-1 == pos)
		{
			// don't find connection, ignore
			return 0; // ENOENT
		}

		socket = darray_get(&utp->sockets, pos);
		assert(socket->utp == utp);
		darray_erase(&utp->sockets, pos);
		utp_socket_release(socket);
		return 0;

	case UTP_ST_STATE:
	case UTP_ST_DATA:
		if (-1 == pos)
		{
			// don't find connection, ignore
			return 0; // ENOENT
		}

		socket = darray_get(&utp->sockets, pos);
		assert(socket->utp == utp);

		if (UTP_STATE_SYN == socket->state)
		{
			socket->recv.timestamp = header.timestamp;
			socket->recv.delay = header.delay;
			socket->recv.window_size = header.window_size;
			socket->recv.seq_nr = header.seq_nr;
			socket->recv.ack_nr = header.ack_nr;
			socket->recv.clock = (uint32_t)system_clock();

			utp->handler.onconnect(utp->param, 0, socket);
			return 0;
		}
		else if (UTP_STATE_FIN == socket->state || UTP_STATE_RESET == socket->state)
		{
			socket->recv.timestamp = header.timestamp;
			socket->recv.delay = header.delay;
			socket->recv.window_size = header.window_size;
			socket->recv.seq_nr = header.seq_nr;
			socket->recv.ack_nr = header.ack_nr;
			socket->recv.clock = (uint32_t)system_clock();

			utp->handler.ondisconnect(utp->param, 0, socket);
			return 0;
		}

		r = utp_socket_input(socket, &header, data + r, bytes - r);
		return r;

	default:
		return -1;
	}
}

int utp_connect(struct utp_t* utp, const struct sockaddr_storage* addr)
{
	int r;
	uint16_t connection_id;
	struct utp_socket_t* socket;
	socket = utp_socket_create(utp);
	if (!socket) return ENOMEM;

	// choose one connection id
	do
	{
		connection_id = (uint16_t)rand();
	} while (utp_find_socket(utp, connection_id, addr));

	socket->recv.id = connection_id;
	socket->recv.timestamp = 0;
	socket->recv.delay = 0;
	socket->recv.window_size = 0;
	socket->recv.seq_nr = 0;
	socket->recv.ack_nr = 0;
	socket->recv.clock = 0;

	socket->send.id = connection_id + 1;
	socket->send.timestamp = (uint32_t)system_clock();
	socket->send.delay = 0;
	socket->send.window_size = UTP_WINDOW_SIZE;
	socket->send.seq_nr = (uint16_t)rand(); // The sequence number is initialized to 1
	socket->send.ack_nr = 0;
	socket->send.clock = (uint32_t)system_clock();

	r = utp_socket_connect(socket, addr);
	if (0 == r)
		r = darray_push_back(&utp->sockets, socket, 1);
	else
		utp_socket_release(socket);
	return r;
}

int utp_disconnect(struct utp_socket_t* socket)
{
	return utp_socket_disconnect(socket);
}

int utp_send(struct utp_socket_t* socket, const uint8_t* data, unsigned int bytes)
{
	return utp_socket_send(socket, data, bytes);
}

static struct utp_socket_t* utp_input_connect(struct utp_t* utp, const struct utp_header_t* header, const struct sockaddr_storage* addr)
{
	int r;
	struct utp_socket_t* socket;
	socket = utp_socket_create(utp);
	if (!socket) return NULL;

    // TODO: check blacklist

	memcpy(&socket->addr, addr, sizeof(socket->addr));

	socket->recv.id = header->connection_id + 1;
	socket->recv.timestamp = header->timestamp;
	socket->recv.delay = header->delay;
	socket->recv.window_size = header->window_size;
	socket->recv.seq_nr = header->seq_nr;
	socket->recv.ack_nr = header->ack_nr;
	socket->recv.clock = (uint32_t)system_clock();

	socket->send.id = header->connection_id;
	socket->send.timestamp = 0;
	socket->send.delay = 0;
	socket->send.window_size = UTP_WINDOW_SIZE;
	socket->send.seq_nr = 0;
	socket->send.ack_nr = 0;
	socket->send.clock = 0;

    r = darray_push_back(&utp->sockets, socket, 1);
    return socket;
}

/// @return 0-find, other-not found, pos insert position
static int utp_connection_compare(const struct utp_socket_t* req, const struct utp_socket_t* socket)
{
	int r;
	r = memcmp(&req->addr, &socket->addr, sizeof(req->addr));
	return 0 == r ? req->recv.id - socket->recv.id : r;
}

static int utp_find_socket(struct utp_t* utp, uint16_t connection, const struct sockaddr_storage* addr)
{
	int r;
	void* socket;
	struct utp_socket_t req;

	req.recv.id = connection;
	memcpy(&req.addr, addr, sizeof(req.addr));
	r = bsearch2(&req, utp->sockets.elements, &socket, utp->sockets.count, sizeof(struct utp_socket_t*), utp_connection_compare);
	return 0 == r ? (struct utp_socket_t**)socket - (struct utp_socket_t**)utp->sockets.elements : -1;
}
