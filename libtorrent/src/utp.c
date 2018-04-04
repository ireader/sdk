#include "sys/pollfd.h"
#include "utp.h"
#include "utp-header.h"
#include "utp-internal.h"
#include "udp-buffer.h"
#include "bsearch.h"

#define UTP_WINDOW_SIZE 50000
#define UTP_SOCKET_BUFFER (8*1024*1024)

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
	int r;
	struct utp_header_t header;
	r = utp_header_read(data, bytes, &header);
	if (r < 0)
		return -1;
	assert(20 == r);

	if (UTP_ST_SYN == header.type)
	{
		r = utp_input_connect(utp, &header, addr);
		if(0 == r)
			r = utp_socket_ack(utp); // auto ack connection
	}
	else
	{
		r = utp_input_msg(utp, &header, addr, data + r, bytes - r);
	}
	
	return r;
}

static int utp_input_connect(struct utp_t* utp, const struct utp_header_t* header, const struct sockaddr_storage* addr)
{
	int r, pos;
	struct utp_socket_t* socket;
	socket = (struct utp_socket_t*)malloc(1, sizeof(*socket));
	if (!socket) return ENOMEM;

    // TODO: check blacklist

	socket->utp = utp;
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

	if (0 == utp_find_socket(utp, header->connection_id, addr, &pos)
		|| 0 == utp_find_socket(utp, header->connection_id + 1, addr, &pos))
	{
		// connection id exist
		free(socket);
		return 0; // EEXIST;
	}

    r = darray_push_back(&utp->sockets, socket, 1);
    return r;
}

static int utp_input_msg(struct utp_t* utp, const struct utp_header_t* header, const struct sockaddr_storage* addr)
{
	int r, pos;
	struct utp_socket_t* socket;

	if (0 != utp_find_socket(utp, header->connection_id, addr, &pos)
		|| header->connection_id != utp->sockets[pos]->recv.id)
	{
		return 0; // ENOENT
	}

	socket = darray_get(&utp->sockets, pos);
	switch (header->type)
	{
	case UTP_ST_RESET:
	case UTP_ST_FIN:
        free(socket);
        darray_erase(&utp->sockets, pos);
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
	v = MIN(socket->recv.id, socket->send.id);
	assert(v + 1 == MAX(socket->recv.id, socket->send.id));
	if (*connection == v || *connection == v + 1)
		return 0;	
	return *connection - v;
}

static int utp_find_socket(struct utp_t* utp, uint16_t connection, const struct sockaddr_storage* addr, int* pos)
{
	int r;
	struct utp_socket_t* socket;

	r = bsearch2(connection, utp->sockets.elements, &socket, utp->sockets.count, sizeof(struct utp_socket_t*), utp_connection_compare);
	if (0 == r)
	{
		pos = socket - (struct utp_socket_t*)utp->sockets.elements;
	}
	return r;
}
