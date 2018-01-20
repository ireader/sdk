#ifndef _utp_internal_h_
#define _utp_internal_h_

#include "utp.h"
#include "sockutil.h"
#include "udp-socket.h"
#include "heap.h"

#define N_SEND_BUFFER (64 * 1024)
#define N_RECV_BUFFER (64 * 1024)
#define N_RECV_BITMASK 32

#define UTP_RECV 0
#define UTP_SEND 0
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

enum {
	UTP_STATE_INIT = 0,
	UTP_STATE_SYN,
	UTP_STATE_SYN_ACK,
	UTP_STATE_CONN,
	UTP_STATE_RESET,
	UTP_STATE_FIN,
};

struct utp_t
{
	uint16_t port;
	struct udp_socket_t udp;

	utp_onconnected onconnected;
	void* param;

	struct utp_socket_t** sockets;
	int capacity;
	int count;
};

struct utp_ack_t
{
	int flag; // 0-don't ack, 1-ack
	int type;
	unsigned int pos;
	unsigned int len;
	uint64_t clock;
};

struct utp_socket_t
{
	int state;
	struct utp_t* utp;
	struct sockaddr_storage addr;

	struct
	{
		uint16_t connection;
		uint16_t seq; // latest packet sn
	} headers[2]; // 0-recv, 1-send

	uint32_t base_delay;

	uint32_t peer_delay; // last packet delay
	uint16_t peer_seq_nr; // peer sequential data sn
	uint16_t peer_ack_nr; // the first unack sn
	uint16_t packet_size;

	uint8_t wbuffer[N_SEND_BUFFER];
	const uint8_t* data; // send data
	unsigned int bytes; // send data length
	unsigned int offset;

	struct
	{
		uint8_t buffer[N_RECV_BUFFER];
		unsigned int pos;
		unsigned int len;
	} rb; // recv ring buffer

	struct utp_ack_t ackQ[2][N_RECV_BITMASK];
	
	struct utp_hander_t handler;
	void* param;
};

#endif /* !_utp_internal_h_ */
