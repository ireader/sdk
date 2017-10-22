#ifndef _tracker_internal_h_
#define _tracker_internal_h_

#include "tracker.h"
#include "aio-socket.h"
#include "http-client.h"
#include "uri-parse.h"
#include "aio-recv.h"

struct tracker_t
{
	char path[256];
	uint8_t info_hash[20];
	uint8_t peer_id[20];
	uint16_t port;
	uint64_t downloaded;
	uint64_t left;
	uint64_t uploaded;
	enum tracker_event_t event;

	tracker_onquery onquery;
	void* param;
	int timeout; // ms

	// http only
	http_client_t* http;

	// udp only
	socket_t udp;
	aio_socket_t aio;
	struct aio_recv_t recv;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	uint64_t connection_id;
	uint32_t transaction_id;
	uint8_t buffer[2 * 1024];
};

int tracker_udp(tracker_t* tracker);

#endif /* !_tracker_internal_h_ */
