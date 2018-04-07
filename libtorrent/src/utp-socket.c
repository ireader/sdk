#include "utp-internal.h"
#include "utp-header.h"
#include "utp/utp-window.h"
#include "utp/utp-timeout.h"
#include "sys/system.h"
#include "sys/atomic.h"
#include "bsearch.h"
#include <stdlib.h>
#include <string.h>

// The initial timeout is set to 1000 milliseconds
#define UTP_DEFAULT_TIMEOUT 1000
// the minimum timeout for a packet is 1/2 second
#define UTP_MINIMUM_TIMEOUT 500

// Default max packet size (1500, minus allowance for IP, UDP, UMTP headers)
// (Also, make it a multiple of 4 bytes, just in case that matters.)
//static int s_max_packet_size = 1456; // from Live555 MultiFrameRTPSink.cpp RTP_PAYLOAD_MAX_SIZE
//static size_t s_max_packet_size = 576; // UNIX Network Programming by W. Richard Stevens
static int s_max_packet_size = 1434; // from VLC
static int utp_ack_inert(struct rarray_t* acks, const struct utp_ack_t* ack);
static int utp_socket_send_packet(struct utp_socket_t* socket, const struct utp_ack_t* ack);
static int utp_socket_send_data(struct utp_socket_t* socket);
static uint32_t utp_socket_window_size(struct utp_socket_t* socket);
static void utp_socket_congestion_control(struct utp_socket_t* socket, uint32_t delay);
static void utp_socket_rtt(struct utp_socket_t* socket, uint64_t rtt);

struct utp_socket_t* utp_socket_create(struct utp_t* utp)
{
	struct utp_socket_t* socket;
	socket = (struct utp_socket_t*)malloc(sizeof(*socket) + N_MAX_BUFFER);
	if (!socket) return NULL;

	memset(socket, 0, sizeof(*socket));
	socket->ref = 1;
	socket->utp = utp;
	socket->state = UTP_STATE_INIT;
	socket->timeout = UTP_DEFAULT_TIMEOUT;
	socket->max_window = 8 * 1024;
	socket->recv.rb = ring_buffer_create(N_MAX_BUFFER);
	if (!socket->recv.rb
		|| 0 != rarray_init(&socket->send.acks, sizeof(struct utp_ack_t), N_ACK_BITS)
		|| 0 != rarray_init(&socket->recv.acks, sizeof(struct utp_ack_t), N_ACK_BITS))
	{
		utp_socket_release(socket);
		return NULL;
	}

	return socket;
}

void utp_socket_release(struct utp_socket_t* socket)
{
	if (0 != atomic_decrement32(&socket->ref))
		return;

	//socket->handler.ondestroy(socket->param);
	ring_buffer_destroy(socket->recv.rb);
	rarray_free(&socket->send.acks);
	rarray_free(&socket->recv.acks);
	free(socket);
}

int utp_socket_connect(struct utp_socket_t* socket, const struct sockaddr_storage* addr)
{
	int r;
	struct utp_ack_t ack;

	assert(UTP_STATE_INIT == socket->state);
	if (UTP_STATE_INIT != socket->state)
		return -1; // invalid state

	socket->state = UTP_STATE_SYN;
	memcpy(&socket->addr, addr, sizeof(socket->addr));

	memset(&ack, 0, sizeof(ack));
	ack.clock = system_clock();
	ack.type = UTP_ST_SYN;
	ack.seq = socket->send.seq_nr++;
	rarray_push_back(&socket->send.acks, &ack);
	assert(1 == rarray_count(&socket->send.acks));

	r = utp_socket_send_packet(socket, &ack);
	if (r < 0)
	{
		rarray_pop_back(&socket->send.acks); // clear ack
		socket->state = UTP_STATE_INIT; // reset status
		return r;
	}
	return 0;
}

int utp_socket_disconnect(struct utp_socket_t* socket)
{
	int r;
	struct utp_ack_t ack;

	assert(UTP_STATE_CONN == socket->state);
	if (UTP_STATE_CONN != socket->state)
		return -1; // invalid state

	memset(&ack, 0, sizeof(ack));
	ack.clock = system_clock();
	ack.type = UTP_STATE_FIN;
	ack.seq = socket->send.seq_nr++;
	rarray_push_back(&socket->send.acks, &ack);

	socket->state = UTP_STATE_FIN;
	r = utp_socket_send_packet(socket, &ack);
	if (r < 0)
	{
		rarray_pop_back(&socket->send.acks); // clear ack
		socket->state = UTP_STATE_CONN; // reset status
		return r;
	}
	return 0;
}

int utp_socket_send(struct utp_socket_t* socket, const uint8_t* data, unsigned int bytes)
{
	assert(NULL == socket->send.rb->ptr && 0 == socket->send.rb->capacity);

	// save data
	if (!socket->send.rb->ptr)
		return -1;
	socket->send.rb->ptr = (uint8_t*)data;
	socket->send.rb->capacity = bytes;
	socket->send.rb->offset = 0;
	socket->send.rb->count = 0;

	return utp_socket_send_data(socket);
}

static int utp_socket_handle_recv(struct utp_socket_t* socket, uint16_t ack_nr, const uint8_t* data, unsigned int bytes)
{
	uint64_t clock;
	unsigned int i, lost;
	struct utp_ack_t* ack;
	assert(0 == bytes % 4);

	clock = system_clock();

	// case 1: ack < peer_ack_nr || ack >= utp_send.seq: invalid, discard
	if (ack_nr - socket->recv.ack_nr > socket->send.seq_nr - socket->recv.ack_nr)
	{
		assert(0);
		return -1; // invalid ack
	}

	// when receiving 3 duplicate acks, ack_nr + 1 is assumed to have been lost 
	// (if a packet with that sequence number has been sent).
	if (socket->recv.ack_nr == ack_nr && ack_nr != socket->send.seq_nr)
		socket->packet_loss += 1;
	else
		socket->packet_loss = 0;

	// case 2: ack >= peer_ack_nr: set [peer_ack_nr, ack) acked
	for(; socket->recv.ack_nr < ack_nr; ++socket->recv.ack_nr)
	{
		ack = rarray_front(&socket->send.acks);
		if (!ack) break;

		assert(ack && ack->flag >= 0);
		assert(ack->seq == socket->recv.ack_nr);
		if (1 == ++ack->flag)
		{
			// 1. Every packet that is ACKed, either by falling in the range (last_ack_nr, ack_nr] 
			//    or by explicitly being acked by a Selective ACK message, should be used to 
			//    update an rtt (round trip time) and rtt_var (rtt variance) measurement. 
			// 2. The rtt and rtt_var is only updated for packets that were sent only once
			utp_socket_rtt(socket, clock - ack->clock);
		}

		rarray_pop_front(&socket->send.acks);
	}
	assert(socket->recv.ack_nr >= ack_nr);

	// case 3: ack == peer_ack_nr:
	for (lost = i = 0; i < bytes * 8; i++)
	{
		// The first bit in the mask therefore represents ack_nr + 2. 
		// ack_nr + 1 is assumed to have been dropped or be missing when this packet was sent.
		ack = (struct utp_ack_t*)rarray_get(&socket->send.acks, /*ack_nr - socket->recv.ack_nr + */ 1 + i);
		if (!ack) break;

		assert(ack->flag >= 0);
		// The bitmask has reverse byte order. 
		// The first byte represents packets [ack_nr + 2, ack_nr + 2 + 7] in reverse order. 
		// The least significant bit in the byte represents ack_nr + 2, 
		// the most significant bit in the byte represents ack_nr + 2 + 7. 
		if (data[i / 8] & (1 << (i % 8)))
		{
			if(1 == ++ack->flag)
				utp_socket_rtt(socket, clock - ack->clock);

			// 1. 3 or more packets have been acked past it 
			// 2. Each packet that is acked in the selective ack message counts as one duplicate ack
			++lost;
		}
	}

	if (lost > 3 || socket->packet_loss > 3)
	{
		socket->max_window = utp_packet_loss(socket->max_window);
	}

	return 0;
}

int utp_socket_input(struct utp_socket_t* socket, const struct utp_header_t* header, const uint8_t* data, int bytes)
{
	int r;
	uint8_t len;
	uint8_t extension;
	struct utp_extension_t selective_acks;

	assert(UTP_STATE_CONN == socket->state);
	assert(UTP_ST_DATA == header->type || UTP_ST_STATE == header->type);

	if (header->ack_nr > socket->recv.ack_nr + rarray_count(&socket->send.acks)
		|| header->ack_nr < socket->recv.ack_nr)
	{
		return -1; // invalid ack number
	}

	// handle extension
	memset(&selective_acks, 0, sizeof(selective_acks));
	extension = header->extension;
	while (extension && bytes >= 2)
	{
		len = data[1];
		if (len + 2 > bytes)
			return -1; // invalid packet

		if (1 == extension)
		{
			assert(0 == len % 4);
			selective_acks.type = 1;
			selective_acks.byte = len;
			selective_acks.data = (uint8_t*)data + 2;
		}

		extension = data[0]; // next extension type
		bytes -= 2 + len;
		data += 2 + len;
	}

	// 1. ack send buffer
	r = utp_socket_handle_recv(socket, header->ack_nr, selective_acks.data, selective_acks.byte);
	if (0 != r)
		return r;

	if(socket->recv.ack_nr + 1 >= (uint16_t)socket->send.rb->count && socket->send.rb->offset == socket->send.rb->capacity && socket->send.rb->capacity > 0)
		socket->handler.onsend(socket->utp->param, socket, 0, socket->send.rb->ptr, socket->send.rb->capacity);

	// 2. recv data
	if (bytes > 0)
	{
		// avoid memory copy
		if (header->seq_nr == socket->send.ack_nr + 1)
		{
			++socket->send.ack_nr;
			r = socket->handler.onrecv(socket->param, socket, 0, data, bytes);
		}
		else
		{
			struct utp_ack_t ack;
			memset(&ack, 0, sizeof(ack));
			ack.ptr = socket->recv.rb->ptr + socket->recv.rb->offset; // rb write position
			ack.len = bytes;
			ack.seq = header->seq_nr;
			ack.clock = system_clock();

			if (0 == ring_buffer_write(socket->recv.rb, data, bytes))
			{
				r = utp_ack_inert(&socket->recv.acks, &ack);
			}
			else
			{
				return E2BIG; // ignore packet
			}
		}

		while (rarray_count(&socket->recv.acks) > 0)
		{
			struct utp_ack_t* ack;
			ack = rarray_front(&socket->recv.acks);
			if (ack->seq != socket->send.ack_nr + 1)
				break;

			++socket->send.ack_nr;
			r = socket->handler.onrecv(socket->param, socket, 0, ack->ptr, ack->len);
			rarray_pop_front(&socket->recv.acks);
		}

		r = utp_socket_ack(socket);
	}

	assert(header->connection_id == socket->recv.id);
	socket->recv.clock = system_clock();
//	socket->recv.ack_nr = header->ack_nr;
	socket->recv.seq_nr = header->seq_nr;
	socket->recv.delay = header->delay;
	socket->recv.timestamp = header->timestamp;
	socket->recv.window_size = header->window_size;

	// calculate peer network delay
	socket->send.delay = (uint32_t)socket->recv.clock - header->timestamp;

	utp_socket_congestion_control(socket, header->delay);
	return 0;
}

int utp_socket_ack(struct utp_socket_t* socket)
{
	int i, n, r;
	uint16_t idx;
	uint8_t data[20 + 2 + N_ACK_BITS / 8];
	struct utp_ack_t* ack;
	struct utp_header_t header;

	header.type = UTP_ST_STATE;
	header.ack_nr = socket->send.ack_nr;
	header.seq_nr = socket->send.seq_nr;
	header.window_size = utp_socket_window_size(socket);
	header.connection_id = socket->send.id;
	header.timestamp = (uint32_t)system_clock();
	header.delay = socket->send.delay;
	header.extension = 0;

	n = utp_header_write(&header, data, sizeof(data));
	assert(20 == n);

	// selective acks
	memset(data + n + 2, 0, N_ACK_BITS / 8);
	for (i = 0; i < rarray_count(&socket->recv.acks); i++)
	{
		ack = rarray_get(&socket->recv.acks, i);
		idx = (uint16_t)(ack->seq - header.ack_nr) - 1;
		assert(idx < N_ACK_BITS);
		if(idx >= N_ACK_BITS) continue;
		data[n + 2 + idx / 8] |= 1 << (idx % 8);
	}

	if (i > 0)
	{
		idx %= N_ACK_BITS;
		header.extension = 1;
		data[n + 2] = 0; // terminate extension list
		data[n + 3] = (uint8_t)((idx + 31) / 32); // a bitmask of at least 32 bits, in multiples of 32 bits.
		n += 2 + data[n + 3];
	}

	r = udp_socket_sendto(&socket->utp->udp, data, n, &socket->addr);
	return r < 0 ? r : (r != n ? -1 : 0);
}

int utp_socket_timer(struct utp_socket_t* socket)
{
	int i;
	uint64_t clock;
	struct utp_ack_t *ack;

	clock = system_clock();
	for (i = 0; i < rarray_count(&socket->send.acks); i++)
	{
		ack = rarray_get(&socket->send.acks, i);
		if (ack->clock + socket->timeout < clock)
		{
			// trigger timeout

			// 1. It will set its packet_size and max_window to the smallest packet size (150 bytes). 
			//    This allows it to send one more packet, and this is how the socket gets started again 
			//    if the window size goes down to zero.
			// 2. The initial timeout is set to 1000 milliseconds, and later updated according to the formula above. 
			//    For every packet consecutive subsequent packet that times out, the timeout is doubled.
			socket->max_window = 150;
			socket->timeout *= 2;

			// retransmit packet
			ack->clock = clock;
			utp_socket_send_packet(socket, ack);
		}
	}
	return 0;
}

static int utp_socket_send_packet(struct utp_socket_t* socket, const struct utp_ack_t* ack)
{
	int r, n;
	uint8_t data[32];
	socket_bufvec_t vec[2];
	struct utp_header_t header;

	header.type = ack->type;
	header.seq_nr = ack->seq;
	header.ack_nr = socket->send.ack_nr;
	header.window_size = utp_socket_window_size(socket);
	header.connection_id = socket->send.id;
	header.timestamp = (uint32_t)ack->clock;
	header.delay = socket->send.delay;
	header.extension = 0;

	n = utp_header_write(&header, data, sizeof(data));
	assert(20 == n);

	socket_setbufvec(vec, 0, data, n);
	socket_setbufvec(vec, 1, ack->ptr, ack->len);
	r = udp_socket_sendto_v(&socket->utp->udp, vec, ack->len > 0 ? 2 : 1, &socket->addr);

	assert(r == n + ack->len);
	return r < 0 ? r : (r != n + ack->len ? -1 : n + ack->len);
}

static int utp_socket_send_data(struct utp_socket_t* socket)
{
	int i, r;
	int flight;
	int32_t max_window;
	int32_t packet_size;
	struct utp_ack_t ack, *pack;

	for (flight = i = 0; i < rarray_count(&socket->send.acks); i++)
	{
		pack = rarray_get(&socket->send.acks, i);
		flight += pack->len + 20 /* utp header */ + 10 /* selective acks */;
	}

	max_window = min(socket->max_window, (int32_t)socket->recv.window_size);
	packet_size = min(max_window - flight, s_max_packet_size);

	while (flight + packet_size < max_window && rarray_count(&socket->send.acks) < N_ACK_BITS)
	{
		memset(&ack, 0, sizeof(ack));
		ack.clock = system_clock();
		ack.type = UTP_ST_DATA;
		ack.seq = socket->send.seq_nr++;
		ack.ptr = socket->send.rb->ptr + socket->send.rb->offset;
		ack.len = min(packet_size, (int)(socket->send.rb->capacity - socket->send.rb->offset));
		
		socket->send.rb->offset += ack.len; // update offset
		rarray_push_back(&socket->send.acks, &ack);

		r = utp_socket_send_packet(socket, &ack);
		if (r < 0)
		{
			// try it later
//			rarray_pop_back(&socket->send.acks);
//			socket->send.rb->offset -= ack.len; // update offset
			return r;
		}

		flight += r;
	}

	return 0;
}

static int utp_ack_compare(const struct utp_ack_t* key, const struct utp_ack_t* elt)
{
	return key->seq - elt->seq;
}

static int utp_ack_inert(struct rarray_t* acks, const struct utp_ack_t* ack)
{
	void* pos;
	if (0 != bsearch2(ack, acks->elements, &pos, acks->count, acks->size, utp_ack_compare))
		return rarray_insert(acks, (struct utp_ack_t*)pos - (struct utp_ack_t*)acks->elements, ack);
	return EEXIST;
}

static uint32_t utp_socket_window_size(struct utp_socket_t* socket)
{
	return socket->recv.rb->capacity - socket->recv.rb->count;
}

// Every packet that is ACKed, either by falling in the range (last_ack_nr, ack_nr] 
// or by explicitly being acked by a Selective ACK message, 
// should be used to update an rtt (round trip time) and rtt_var (rtt variance) measurement. 
static void utp_socket_rtt(struct utp_socket_t* socket, uint64_t rtt)
{
	socket->timeout = utp_timeout(socket->rtt, socket->rtt_var, (uint32_t)rtt);
}

static void utp_socket_congestion_control(struct utp_socket_t* socket, uint32_t delay)
{
	int i, flight;
	uint32_t base_delay;
	struct utp_ack_t* ack;

	utp_delay_push(&socket->delay, delay);
	base_delay = utp_delay_get(&socket->delay);

	// outstanding_packet
	for (flight = i = 0; i < rarray_count(&socket->send.acks); i++)
	{
		ack = rarray_get(&socket->recv.acks, i);
		flight += ack->len + 20 /* utp header */ + 10 /* selective acks */;
	}

	// update max_window size
	socket->max_window = utp_max_window(base_delay, delay, flight, socket->max_window);
}
