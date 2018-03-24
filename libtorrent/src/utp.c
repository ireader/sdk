#include "sys/poll.h"
#include "utp.h"
#include "utp-header.h"
#include "utp-internal.h"
#include "udp-buffer.h"
#include "bsearch.h"

#define UTP_WINDOW_SIZE 50000
#define UTP_SOCKET_BUFFER (8*1024*1024)

struct utp_t* utp_create(const uint16_t port, utp_onconnected onconnected, void* param)
{
    struct utp_t* utp;
    utp = (struct utp_t*)calloc(1, sizeof(*utp));
    if (!utp) return NULL;

    utp->ptr = udp_buffer_create(UTP_SOCKET_BUFFER);
    utp->onconnected = onconnected;
    utp->param = param;

    if (!utp->ptr || 0 != udp_socket_create(port, &utp->udp))
    {
        utp_destroy(utp);
        utp = NULL;
    }

    return utp;
}

void utp_destroy(struct utp_t* utp)
{
	udp_socket_destroy(&utp->udp);

    if (utp->ptr)
    {
        udp_socket_destroy(utp->ptr);
        utp->ptr = NULL;
    }

	if (utp->sockets)
	{
		assert(utp->capacity > 0);
		free(utp->sockets);
		utp->sockets = NULL;
	}

	free(utp);
}

int utp_process(struct utp_t* utp)
{
    int i, r;
    struct pollfd fds[2];

    fds[0].fd = utp->udp.udp;
    fds[1].fd = utp->udp.udp6;
    for (i = 0; i < 2; i++)
    {
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }

    r = poll(fds, 2, timeout);
    while (-1 == r && EINTR == errno)
        r = poll(fds, 2, timeout);

    if (r <= 0)
        return r;

    for(i = 0; i < 2;i++)
    {
        if (0 == fds[i].revents)
            continue;
        
        utp_read(fds[i].fd);
    }
    return r;
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
		r = utp_input_connect(utp, &header, addr);
		if(0 == r)
			r = utp_socket_ack(utp);
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

	utp_insert_socket(utp, socket, pos);

	r = utp->onconnected(utp->param, socket);
	if (0 != r)
	{
		free(socket);
	}

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
		ptr = realloc(utp->sockets, sizeof(utp->sockets[0]) * (utp->capacity + 50));
		if (!ptr) return ENOMEM;

		utp->sockets = (struct utp_socket_t**)ptr;
		utp->capacity += 50;
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
