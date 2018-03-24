#include "utp-internal.h"
#include "utp-header.h"
#include "sys/system.h"
#include <stdlib.h>
#include <string.h>

struct utp_socket_t* utp_socket_create(struct utp_t* utp)
{
	struct utp_socket_t* socket;
	socket = (struct utp_socket_t*)calloc(1, sizeof(*socket));
	if (!socket) return ENOMEM;

	socket->utp = utp;
	socket->state = UTP_STATE_INIT;
	socket->peer_delay = 0;
}

void utp_socket_destroy(struct utp_socket_t* socket)
{
	free(socket);
}

void utp_socket_sethandler(struct utp_socket_t* socket, struct utp_hander_t* handler, void* param)
{
	memcpy(&socket->handler, handler, sizeof(socket->handler));
	socket->param = param;
}

int utp_socket_connect(struct utp_socket_t* socket, const struct sockaddr_storage* addr)
{
	int r, pos;
	uint16_t connection;
	struct utp_ack_t* ack;
	struct utp_header_t header;

	if (UTP_STATE_INIT != socket->state)
		return -1; // invalid state

	memcpy(&socket->addr, addr, sizeof(socket->addr));

	// generate connection id
	do
	{
		connection = (uint16_t)rand();
		socket->recv.id = connection;
		socket->send.id = connection + 1;
	} while (0 != utp_insert_socket(socket->utp, socket));

	socket->recv.seq_nr = 0;
	socket->send.seq_nr = (uint16_t)rand(); // The sequence number is initialized to 1
	socket->peer_ack_nr = socket->headers[UTP_SEND].seq;
	socket->peer_delay = 0;

	// push task
	ack = &socket->ackQ[UTP_SEND][socket->headers[UTP_SEND].seq % N_RECV_BITMASK];
	memset(ack, 0, sizeof(*ack));
	ack->type = UTP_ST_SYN;
	ack->flag = 0;

	// send
	socket->state = UTP_STATE_SYN;
	r = utp_socket_send_connect(socket);
	if (0 == r)
	{
		// pop task
		socket->state = UTP_STATE_INIT;
		ack->flag = 1; // clear ack flag
	}

	return r;
}

int utp_socket_send(struct utp_socket_t* socket, const uint8_t* data, unsigned int bytes)
{
	// save data
	assert(NULL == socket->data && 0 == socket->bytes);
	socket->data = data;
	socket->bytes = bytes;

	return utp_socket_send_data(socket);
}

static int utp_socket_selective_ack(struct utp_socket_t* socket, uint16_t ack_nr, const uint8_t* data, unsigned int bytes)
{
	uint16_t dack;
	unsigned int i;
	struct utp_ack_t* ack;
	assert(0 == bytes / 4);

	dack = ack_nr - socket->peer_ack_nr;

	// case 1: ack < peer_ack_nr || ack >= utp_send.seq: invalid, discard
	assert(dack < socket->headers[UTP_SEND].seq - socket->peer_ack_nr);

	// case 2: ack >= peer_ack_nr: set [peer_ack_nr, ack) acked
	for (i = 0; i < dack; i++)
	{
		ack = &socket->ackQ[UTP_SEND][(socket->peer_ack_nr + i) % N_RECV_BITMASK];
		ack->flag = 1;
	}

	// case 3: ack == peer_ack_nr:
	for (i = 0; i < bytes * 8 && i + 2 < socket->headers[UTP_SEND].seq - ack_nr; i++)
	{
		// The first bit in the mask therefore represents ack_nr + 2. 
		// ack_nr + 1 is assumed to have been dropped or be missing when this packet was sent.
		ack = &socket->ackQ[UTP_SEND][(ack_nr + 2 + i) % N_RECV_BITMASK];

		// The bitmask has reverse byte order. 
		// The first byte represents packets [ack_nr + 2, ack_nr + 2 + 7] in reverse order. 
		// The least significant bit in the byte represents ack_nr + 2, 
		// the most significant bit in the byte represents ack_nr + 2 + 7. 
		ack->flag = data[i / 8] & (1 << (i % 8));
	}

	return 0;
}

static int utp_socket_ack(struct utp_socket_t* socket, const struct utp_header_t* header, const uint8_t* data, unsigned int bytes)
{
	uint16_t len;
	uint16_t extension;
	struct utp_ack_t* ack;

	if (header->ack_nr - socket->peer_ack_nr >= socket->headers[UTP_SEND].seq - socket->peer_ack_nr)
	{
		return -1; // invalid ack number
	}

	// handle extension
	extension = header->extension;
	while (extension && bytes >= 4)
	{
		extension = (data[0] << 8) | data[1];
		len = (data[2] << 8) | data[3];
		if (len + 4 > bytes)
			return -1; // invalid packet

		if (1 == extension)
		{
			assert(0 == len % 4);
			assert(0 == bytes - 4 - len);
			assert(UTP_ST_STATE == header->type);
			utp_socket_selective_ack(socket, header->ack_nr, data + 4, len);
		}

		data += 4 + len;
		bytes -= 4 + len;
	}

	// mark acked
	socket->ackQ[UTP_SEND][header->ack_nr % N_RECV_BITMASK].flag = 1;

	while (socket->peer_ack_nr != socket->headers[UTP_SEND].seq)
	{
		ack = &socket->ackQ[UTP_SEND][socket->peer_ack_nr % N_RECV_BITMASK];
		if (0 == ack->flag)
			break;

		ack->flag = 0; // clear flags
		++socket->peer_ack_nr;

		if (UTP_ST_SYN == ack->type)
		{
			assert(UTP_STATE_SYN == socket->state);
			socket->state = UTP_STATE_CONN;
			socket->handler.onconnect(socket->param);
		}
		else if (UTP_ST_DATA == ack->type)
		{
			if (ack->pos + ack->len == socket->bytes)
			{
				// send done
				socket->handler.onsend(socket->packet_size, 0, socket->data, socket->bytes);
			}
		}
		else
		{
			// do nothing
		}
	}

	return 0;
}

static int utp_socket_data(struct utp_socket_t* socket, const struct utp_header_t* header, const uint8_t* data, unsigned int bytes)
{
	int r;
	unsigned int pos;
	uint16_t dseq, seq;
	struct utp_ack_t* ack;

	assert(UTP_ST_SYN != header->type);
	assert(socket->headers[UTP_RECV].seq - socket->peer_seq_nr < N_RECV_BITMASK);
	assert(socket->headers[UTP_SEND].seq - socket->peer_ack_nr < N_RECV_BITMASK);
	dseq = header->seq_nr - socket->peer_seq_nr;
	if (dseq > N_RECV_BITMASK)
	{
		return -1; // invalid seq
	}

	r = utp_socket_ack(socket, header, data, bytes);
	if (0 != r)
		return r;

	socket->peer_delay = (uint32_t)system_clock() - header->timestamp;

	if (header->seq_nr - socket->headers[UTP_RECV].seq < N_RECV_BITMASK)
	{
		socket->headers[UTP_RECV].seq = header->seq_nr; // update latest seq
	}

	if (header->seq_nr == socket->peer_seq_nr)
	{
		socket->peer_seq_nr += 1;
		utp_socket_send_ack(socket);
		r = socket->handler.onrecv(socket->param, 0, data, bytes);

		while (socket->headers[UTP_RECV].seq - socket->peer_seq_nr < N_RECV_BITMASK)
		{
			ack = &socket->ackQ[UTP_RECV][socket->peer_seq_nr % N_RECV_BITMASK];
			if (0 == ack->flag)
				break;

			ack->flag = 0; // clear flags
			++socket->peer_seq_nr;

			assert(ack->pos == socket->rb.pos);
			assert(ack->len <= socket->rb.len);
			socket->rb.len -= ack->len;
			socket->rb.pos += ack->len;
			r = socket->handler.onrecv(socket->param, 0, socket->rb.buffer + ack->pos, ack->len);
		}
	}
	else
	{
		if (socket->rb.len + bytes > sizeof(socket->rb.buffer))
		{
			assert(0);
			return -1; // too big, discard
		}

		pos = (socket->rb.pos + socket->rb.len) % sizeof(socket->rb.buffer);
		socket->ackQ[UTP_RECV][header->seq_nr % N_RECV_BITMASK].flag = 1;
		socket->ackQ[UTP_RECV][header->seq_nr % N_RECV_BITMASK].pos = pos;
		socket->ackQ[UTP_RECV][header->seq_nr % N_RECV_BITMASK].len = bytes;
		memcpy(socket->rb.buffer + pos, data, bytes);
		socket->rb.len += bytes;

		return utp_socket_send_ack_select(socket);
	}

	return 0;
}

static int utp_socket_send_connect(struct utp_socket_t* socket)
{
	int n, r;
	struct utp_header_t header;

	header.type = UTP_ST_SYN;
	header.ack_nr = 0;
	header.seq_nr = socket->headers[UTP_SEND].seq++;
	header.window_size = sizeof(socket->rb.buffer);
	header.connection_id = socket->headers[UTP_RECV].connection;
	header.timestamp = (uint32_t)system_clock();
	header.delay = 0;
	header.extension = 0;
	n = utp_header_write(&header, socket->wbuffer, sizeof(socket->wbuffer));
	assert(20 == n);

	r = udp_socket_sendto(&socket->utp->udp, socket->wbuffer, n, &socket->addr);
	return r < 0 ? r : (r != n ? -1 : 0);
}

static int utp_socket_send_ack(struct utp_socket_t* socket)
{
	int n, r;
	struct utp_header_t header;

	header.type = UTP_ST_STATE;
	header.ack_nr = socket->headers[UTP_RECV].seq;
	header.seq_nr = socket->headers[UTP_SEND].seq;
	header.window_size = sizeof(socket->rb.buffer) - socket->rb.len;
	header.connection_id = socket->headers[UTP_SEND].connection;
	header.timestamp = (uint32_t)system_clock();
	header.delay = socket->peer_delay;
	header.extension = 0;
	n = utp_header_write(&header, socket->wbuffer, sizeof(socket->wbuffer));
	assert(20 == n);

	r = udp_socket_sendto(&socket->utp->udp, socket->wbuffer, n, &socket->addr);
	return r < 0 ? r : (r != n ? -1 : 0);
}

static int utp_socket_send_ack_select(struct utp_socket_t* socket)
{
	int n, r, i;
	struct utp_ack_t* ack;
	struct utp_header_t header;

	header.type = UTP_ST_STATE;
//	header.ack_nr = socket->lost_packet_nr;
	header.seq_nr = socket->headers[UTP_SEND].seq;
	header.window_size = sizeof(socket->rb.buffer) - socket->rb.len;
	header.connection_id = socket->headers[UTP_SEND].connection;
	header.timestamp = (uint32_t)system_clock();
	header.delay = socket->peer_delay;
	header.extension = 1; // Selective acks
	n = utp_header_write(&header, socket->wbuffer, sizeof(socket->wbuffer));
	assert(20 == n);

	assert(0 == N_RECV_BITMASK % 32);
	i = N_RECV_BITMASK / 8;
	socket->wbuffer[0] = 0; // terminate extension list
	socket->wbuffer[1] = 0;
	socket->wbuffer[2] = (uint8_t)(i >> 8);
	socket->wbuffer[3] = (uint8_t)i;

	memset(&socket->wbuffer[n + 4], 0, N_RECV_BITMASK / 8);
	for (i = 0; i < N_RECV_BITMASK / 8; i++)
	{
//		ack = &socket->ackQ[UTP_RECV][(i + socket->lost_packet_nr) % N_RECV_BITMASK];
		socket->wbuffer[n + 4 + i / 8] |= (ack->flag ? 0 : 1) << (i % 8);
	}

	r = udp_socket_sendto(&socket->utp->udp, socket->wbuffer, n, &socket->addr);
	return r < 0 ? r : (r != n + 4 + N_RECV_BITMASK / 8 ? -1 : 0);
}

static int utp_socket_send_data(struct utp_socket_t* socket, const void* data, unsigned int bytes)
{
	int n, r;
	struct utp_ack_t* ack;
	struct utp_header_t header;

	ack = &socket->ackQ[UTP_SEND][socket->headers[UTP_SEND].seq % N_RECV_BITMASK];
	assert(0 == ack->flag);
	ack->type = UTP_ST_DATA;
	ack->flag = 0;
	ack->clock = system_clock();
	ack->len = MIN(socket->packet_size, socket->bytes - socket->offset);
	ack->pos = socket->offset;
	socket->offset += r;
	
	header.type = UTP_ST_DATA;
	header.ack_nr = socket->headers[UTP_RECV].seq;
	header.seq_nr = socket->headers[UTP_SEND].seq++;
	header.window_size = sizeof(socket->rb.buffer) - socket->rb.len;
	header.connection_id = socket->headers[UTP_SEND].connection;
	header.timestamp = (uint32_t)system_clock();
	header.delay = socket->peer_delay;
	header.extension = 0;
	n = utp_header_write(&header, socket->wbuffer, sizeof(socket->wbuffer));
	assert(20 == n);

	// send data(fragment)
	memcpy(socket->wbuffer + n, data, bytes);
	n += bytes;

	r = udp_socket_sendto(&socket->utp->udp, socket->wbuffer, n, &socket->addr);
	return r < 0 ? r : (r != n ? -1 : 0);
}
