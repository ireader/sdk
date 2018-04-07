#ifndef _utp_internal_h_
#define _utp_internal_h_

#include "utp.h"
#include "sockutil.h"
#include "udp-socket.h"
#include "udp-buffer.h"
#include "utp-header.h"
#include "ring-buffer.h"
#include "utp/utp-delay.h"
#include "darray.h"
#include "rarray.h"
#include "heap.h"

#define N_MAX_BUFFER (64 * 1024)
#define N_ACK_BITS 64

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

enum {
	UTP_STATE_INIT = 0,
	UTP_STATE_SYN,
//	UTP_STATE_SYN_ACK,
	UTP_STATE_CONN,
	UTP_STATE_RESET,
	UTP_STATE_FIN,
};

struct utp_t
{
	uint16_t port;
	struct udp_socket_t udp;
    struct udp_buffer_t* ptr;

    struct utp_hander_t handler;
	void* param;

    struct darray_t sockets; // struct utp_socket_t**
};

struct utp_extension_t
{
	uint8_t  type;
	uint8_t  byte;
	uint8_t* data;
};

struct utp_ack_t
{
	uint8_t* ptr;
	int len;

	int flag; // 0-don't ack, 1-ack
	int type;
	uint16_t seq;
	uint64_t clock;
};

struct utp_connection_t
{
	uint16_t id;
	uint32_t timestamp;
	uint32_t delay;
	uint32_t window_size;
	uint16_t seq_nr;
	uint16_t ack_nr;

	uint64_t clock; // last recv/send clock
	struct rarray_t acks; // struct utp_ack_t array
	
	// send/recv buffer
	struct ring_buffer_t* rb;
};

struct utp_socket_t
{
	int32_t ref;

	int state;
	struct utp_t* utp;
	struct sockaddr_storage addr;
	struct utp_connection_t send;
	struct utp_connection_t recv;

	int packet_loss;
	int32_t rtt_var;
	int32_t rtt;
	struct utp_delay_t delay; // calculate base_delay
	int32_t base_delay;
	int32_t max_window;
	int32_t packet_size;

	//uint32_t peer_delay; // last packet delay
	//uint16_t peer_seq_nr; // peer sequential data sn
	//uint16_t peer_ack_nr; // the first unack sn

	struct utp_hander_t handler;
	void* param;
};

struct utp_socket_t* utp_socket_create(struct utp_t* utp);
void utp_socket_release(struct utp_socket_t* socket);
int utp_socket_connect(struct utp_socket_t* socket, const struct sockaddr_storage* addr);
int utp_socket_disconnect(struct utp_socket_t* socket);
int utp_socket_input(struct utp_socket_t* socket, const struct utp_header_t* header, const uint8_t* data, int bytes);
int utp_socket_send(struct utp_socket_t* socket, const uint8_t* data, unsigned int bytes);
int utp_socket_send_ack(struct utp_socket_t* socket);

#endif /* !_utp_internal_h_ */
