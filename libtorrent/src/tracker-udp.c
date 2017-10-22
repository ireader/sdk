// http://www.bittorrent.org/beps/bep_0015.html (UDP Tracker Protocol for BitTorrent)
// http://www.bittorrent.org/beps/bep_0041.html (UDP Tracker Protocol Extensions)

#include "tracker.h"
#include "tracker-internal.h"
#include "aio-recv.h"
#include "byte-order.h"
#include "sys/system.h"
#include <stdlib.h>

#define TIMEOUT 5000

enum tracker_action_t
{
	TRACKER_ACTION_CONNECT = 0,
	TRACKER_ACTION_ANNOUNCE,
	TRACKER_ACTION_SCRAPE,
	TRACKER_ACTION_ERROR,
};

struct tracker_scrape_t
{
	uint32_t senders;
	uint32_t completed;
	uint32_t leechers;
};

/// @return <0-error, other-buffer size
static int tracker_udp_connect_write(uint8_t buffer[16], uint32_t transaction_id);
/// @return 0-ok, other-error
static int tracker_udp_connect_read(const uint8_t* buffer, size_t bytes, uint32_t* transaction_id);
/// @return <0-error, other-buffer size
static int tracker_udp_connect_reply_write(uint8_t buffer[16], uint32_t transaction_id, uint64_t connection_id);
/// @return 0-ok, other-error
static int tracker_udp_connect_reply_read(const uint8_t* buffer, size_t bytes, uint32_t *transaction_id, uint64_t *connection_id);
/// @return <0-error, other-buffer size
static int tracker_udp_announce_write(uint8_t* buffer, size_t bytes, uint64_t connection_id, uint32_t transaction_id, const uint8_t info_hash[20], const uint8_t peer_id[20], uint64_t downloaded, uint64_t left, uint64_t uploaded, int event, uint16_t port, const char* path);
/// @return 0-ok, other-error
static int tracker_udp_announce_read(const uint8_t* buffer, size_t bytes, uint64_t *connection_id, uint32_t *transaction_id, uint8_t info_hash[20], uint8_t peer_id[20], uint64_t *downloaded, uint64_t *left, uint64_t *uploaded, int *event, uint16_t *port, char** path);
/// @return <0-error, other-buffer size
static int tracker_udp_announce_reply_write(uint8_t* buffer, size_t bytes, uint32_t transaction_id, uint32_t interval, uint32_t leechers, uint32_t senders, const struct sockaddr_storage* peers, size_t count);
/// @return <0-error, >=0-peer count
static int tracker_udp_announce_reply_read(const uint8_t* buffer, size_t bytes, int ipv6, uint32_t *transaction_id, uint32_t *interval, uint32_t *leechers, uint32_t *senders, struct sockaddr_storage** peers);
/// @return <0-error, other-buffer size
static int tracker_udp_scrape_write(uint8_t* buffer, size_t bytes, uint64_t connection_id, uint32_t transaction_id, const uint8_t info_hash[][20], size_t count);
/// @return 0-ok, other-error
static int tracker_udp_scrape_read(const uint8_t* buffer, size_t bytes, uint64_t *connection_id, uint32_t *transaction_id, uint8_t info_hash[][20]);
/// @return <0-error, other-buffer size
static int tracker_udp_scrape_reply_write(uint8_t* buffer, size_t bytes, uint32_t transaction_id, const struct tracker_scrape_t* scrapers, size_t count);
/// @return <0-error, >=0-scrapers count
static int tracker_udp_scrape_reply_read(const uint8_t* buffer, size_t bytes, uint32_t *transaction_id, struct tracker_scrape_t** scrapers);


// Connect

/// @return <0-error, other-buffer size
static int tracker_udp_connect_write(uint8_t buffer[16], uint32_t transaction_id)
{
	const uint64_t protocol_id = 0x41727101980; // magic constant
	nbo_w64(buffer, protocol_id);
	nbo_w32(buffer + 8, TRACKER_ACTION_CONNECT);
	nbo_w32(buffer + 12, transaction_id);
	return 16;
}

/// @return 0-ok, other-error
static int tracker_udp_connect_read(const uint8_t* buffer, size_t bytes, uint32_t* transaction_id)
{
	uint64_t protocol_id;
	uint32_t action;
	if (bytes < 16)
		return -1;

	nbo_r64(buffer, &protocol_id);
	nbo_r32(buffer + 8, &action);
	nbo_r32(buffer + 12, transaction_id);

	return (TRACKER_ACTION_CONNECT == action && 0x41727101980 == protocol_id) ? 0 : -1;
}

/// @return <0-error, other-buffer size
static int tracker_udp_connect_reply_write(uint8_t buffer[16], uint32_t transaction_id, uint64_t connection_id)
{
	nbo_w32(buffer, TRACKER_ACTION_CONNECT);
	nbo_w32(buffer + 4, transaction_id);
	nbo_w64(buffer + 8, connection_id);
	return 16;
}

/// @return 0-ok, other-error
static int tracker_udp_connect_reply_read(const uint8_t* buffer, size_t bytes, uint32_t *transaction_id, uint64_t *connection_id)
{
	uint32_t action;
	if (bytes < 4)
		return -1;

	nbo_r32(buffer, &action); // maybe error
	if (TRACKER_ACTION_CONNECT != action || bytes < 16)
		return -1;

	nbo_r32(buffer + 4, transaction_id); 
	nbo_r64(buffer + 8, connection_id);
	return 0;
}

// Announce

/// @return <0-error, other-buffer size
static int tracker_udp_announce_write(uint8_t* buffer, size_t bytes, uint64_t connection_id, uint32_t transaction_id, const uint8_t info_hash[20], const uint8_t peer_id[20], uint64_t downloaded, uint64_t left, uint64_t uploaded, int event, uint16_t port, const char* path)
{
	size_t n;
	if (bytes < 98)
		return -1;

	nbo_w64(buffer, connection_id);
	nbo_w32(buffer + 8, TRACKER_ACTION_ANNOUNCE);
	nbo_w32(buffer + 12, transaction_id);
	memcpy(buffer + 16, info_hash, 20);
	memcpy(buffer + 36, peer_id, 20);
	nbo_w64(buffer + 56, downloaded);
	nbo_w64(buffer + 64, left);
	nbo_w64(buffer + 72, uploaded);
	nbo_w32(buffer + 80, event); // 0: none; 1: completed; 2: started; 3: stopped
	nbo_w32(buffer + 84, 0); // ip, default 0
	nbo_w32(buffer + 88, 0); // key
	nbo_w32(buffer + 92, 0xFFFFFFFF); // num_want, default -1
	nbo_w16(buffer + 96, port);

	// http://www.bittorrent.org/beps/bep_0041.html
	// URLData: <Option-Type 0x2>, <Length Byte>, <Variable-Length URL Data>
	if (path)
	{
		n = strlen(path);
		if (98 + ((n + 254) / 255) * 2 + n > bytes)
			return -1;

		buffer += 98;
		while (n > 0)
		{
			buffer[0] = 0x02; // URLData
			buffer[1] = (uint8_t)(n > 255 ? 255 : n); // Length Byte
			memcpy(buffer + 2, path, buffer[1]);
			buffer += 2 + buffer[1];
			path += buffer[1];
			n -= buffer[1];
		}
		return 98 + ((n + 254) / 255) * 2 + n;
	}

	return 98;
}

/// @return 0-ok, other-error
static int tracker_udp_announce_read(const uint8_t* buffer, size_t bytes, uint64_t *connection_id, uint32_t *transaction_id, uint8_t info_hash[20], uint8_t peer_id[20], uint64_t *downloaded, uint64_t *left, uint64_t *uploaded, int *event, uint16_t *port, char** path)
{
	size_t n, len;
	uint32_t action;
	if (bytes < 98)
		return -1;

	nbo_r64(buffer, connection_id);
	nbo_r32(buffer + 8, &action);
	nbo_r32(buffer + 12, transaction_id);
	memcpy(info_hash, buffer + 16, 20);
	memcpy(peer_id, buffer + 36, 20);
	nbo_r64(buffer + 56, downloaded);
	nbo_r64(buffer + 64, left);
	nbo_r64(buffer + 72, uploaded);
	nbo_r32(buffer + 80, (uint32_t*)event); // 0: none; 1: completed; 2: started; 3: stopped
//	nbo_r32(buffer + 84, 0); // ip, default 0
//	nbo_r32(buffer + 88, 0); // key
//	nbo_r32(buffer + 92, 0xFFFFFFFF); // num_want, default -1
	nbo_r16(buffer + 96, port);

	if (TRACKER_ACTION_CONNECT != action)
		return -1;

	len = 0;
	*path = NULL;
	for (n = 98; n < bytes; n++)
	{
		switch (buffer[n])
		{
		case 0x00: // EndOfOptions
			return 0; // stop parse

		case 0x01: // NOP
			break;

		case 0x02: // URLData
			if (n + 1 >= bytes || n + 1 + buffer[n + 1] > bytes)
				return -1;

			*path = realloc(*path, len + buffer[n + 1] + 1);
			memcpy(*path + len, buffer + n + 2, buffer[n + 1]);
			(*path)[len + buffer[n + 1]] = 0;
			n += 1 + buffer[n + 1];
			len += buffer[n + 1];
			break;

		default:
			assert(0);
			return 0; // unknown command
		}
	}

	return 0;
}

/// @return <0-error, other-buffer size
static int tracker_udp_announce_reply_write(uint8_t* buffer, size_t bytes, uint32_t transaction_id, uint32_t interval, uint32_t leechers, uint32_t senders, const struct sockaddr_storage* peers, size_t count)
{
	size_t i;

	if (bytes < 20 + (count > 0 ? (count * ((AF_INET6 == peers[0].ss_family) ? 18 : 6)) : 0))
		return -1;

	nbo_w32(buffer, TRACKER_ACTION_ANNOUNCE);
	nbo_w32(buffer + 4, transaction_id);
	nbo_w32(buffer + 8, interval);
	nbo_w32(buffer + 12, leechers);
	nbo_w32(buffer + 16, senders);

	for (i = 0; i < count; i++)
	{
		if (AF_INET == peers[0].ss_family)
		{
			struct sockaddr_in* addr;
			addr = (struct sockaddr_in*)&peers[i];
			memcpy(buffer + 20 + 6 * i, &addr->sin_addr.s_addr, 4);
			memcpy(buffer + 20 + 6 * i + 4, &addr->sin_port, 2);
		}
		else if(AF_INET6 == peers[0].ss_family)
		{
			struct sockaddr_in6* addr;
			addr = (struct sockaddr_in6*)&peers[i];
			memcpy(buffer + 20 + 18 * i, &addr->sin6_addr.s6_addr, 16);
			memcpy(buffer + 20 + 18 * i + 16, &addr->sin6_port, 2);
		}
		else
		{
			assert(0);
			return -1;
		}
	}

	return 20 + (count > 0 ? (count * ((AF_INET6 == peers[0].ss_family) ? 18 : 6)) : 0);
}

/// @return <0-error, >=0-peer count
static int tracker_udp_announce_reply_read(const uint8_t* buffer, size_t bytes, int ipv6, uint32_t *transaction_id, uint32_t *interval, uint32_t *leechers, uint32_t *senders, struct sockaddr_storage** peers)
{
	size_t i, n;
	uint32_t action;
	if (bytes < 4)
		return -1;

	nbo_r32(buffer, &action); // maybe error
	if (TRACKER_ACTION_ANNOUNCE != action || bytes < 20)
		return -1;

	nbo_r32(buffer + 4, transaction_id);
	nbo_r32(buffer + 8, interval);
	nbo_r32(buffer + 12, leechers);
	nbo_r32(buffer + 16, senders);

	n = (bytes - 20) / (ipv6 ? 18 : 6);
	*peers = calloc(n, sizeof(struct sockaddr_storage));
	if (!*peers)
		return -1;

	buffer += 20;
	for (i = 0; i < n; i++)
	{
		if (ipv6)
		{
			struct sockaddr_in6* addr;
			addr = (struct sockaddr_in6*)((*peers) + i);
			addr->sin6_family = AF_INET6;
			memcpy(&addr->sin6_addr.s6_addr, buffer, 16);
			memcpy(&addr->sin6_port, buffer + 16, 2);
			buffer += 18;
		}
		else
		{
			struct sockaddr_in* addr;
			addr = (struct sockaddr_in*)((*peers) + i);
			addr->sin_family = AF_INET;
			memcpy(&addr->sin_addr.s_addr, buffer, 4);
			memcpy(&addr->sin_port, buffer + 4, 2);
			buffer += 6;
		}
	}

	return n;
}

// Scrape

/// @return <0-error, other-buffer size
static int tracker_udp_scrape_write(uint8_t* buffer, size_t bytes, uint64_t connection_id, uint32_t transaction_id, const uint8_t info_hash[][20], size_t count)
{
	size_t i;
	if (bytes < 16 + 20 * count)
		return -1;

	nbo_w64(buffer, connection_id);
	nbo_w32(buffer + 8, TRACKER_ACTION_SCRAPE);
	nbo_w32(buffer + 12, transaction_id);

	buffer += 16;
	for (i = 0; i < count; i++)
	{
		memcpy(buffer, info_hash[i], 20);
		buffer += 20;
	}

	return 16 + 20 * count;
}

/// @return 0-ok, other-error
static int tracker_udp_scrape_read(const uint8_t* buffer, size_t bytes, uint64_t *connection_id, uint32_t *transaction_id, uint8_t info_hash[][20])
{
	size_t i, n;
	uint32_t action;
	if (bytes < 16 || 0 != (bytes - 16) % 20)
		return -1;

	nbo_r64(buffer, connection_id);
	nbo_r32(buffer + 8, &action);
	nbo_r32(buffer + 12, transaction_id);
	if (TRACKER_ACTION_SCRAPE != action)
		return -1;

	buffer += 12;
	n = (bytes - 16) / 20;
	info_hash = malloc(20 * n);
	for (i = 0; i < n; i++)
	{
		memcpy(info_hash[i], buffer, 20);
		buffer += 20;
	}

	return 0;
}

/// @return <0-error, other-buffer size
static int tracker_udp_scrape_reply_write(uint8_t* buffer, size_t bytes, uint32_t transaction_id, const struct tracker_scrape_t* scrapers, size_t count)
{
	size_t i;
	if (bytes < 8 + 12 * count)
		return -1;

	nbo_w32(buffer + 0, TRACKER_ACTION_SCRAPE);
	nbo_w32(buffer + 4, transaction_id);

	buffer += 8;
	for (i = 0; i < count; i++)
	{
		nbo_w32(buffer + 0, scrapers[i].senders);
		nbo_w32(buffer + 4, scrapers[i].completed);
		nbo_w32(buffer + 8, scrapers[i].leechers);
		buffer += 12;
	}

	return 8 + 12 * count;
}

/// @return <0-error, >=0-scrapers count
static int tracker_udp_scrape_reply_read(const uint8_t* buffer, size_t bytes, uint32_t *transaction_id, struct tracker_scrape_t** scrapers)
{
	size_t i, n;
	uint32_t action;
	if (bytes < 8 || 0 != (bytes - 8) % 12)
		return -1;

	nbo_r32(buffer + 0, &action);
	nbo_r32(buffer + 4, transaction_id);
	if (TRACKER_ACTION_SCRAPE != action)
		return -1;

	buffer += 8;
	n = (bytes - 8) / 12;
	scrapers = malloc(sizeof(struct tracker_scrape_t) * n);
	for (i = 0; i < n; i++)
	{
		nbo_r32(buffer + 0, &scrapers[i]->senders);
		nbo_r32(buffer + 4, &scrapers[i]->completed);
		nbo_r32(buffer + 8, &scrapers[i]->leechers);
		buffer += 12;
	}

	return n;
}

static void tracker_udp_onannounce(void* param, int code, size_t bytes, const struct sockaddr* addr, socklen_t addrlen)
{
	int n;
	struct tracker_t* tracker;
	struct tracker_reply_t reply;
	tracker = (struct tracker_t*)param;

	memset(&reply, 0, sizeof(reply));
	if (0 == code)
	{
		assert(addrlen == tracker->addrlen && 0 == memcmp(&tracker->addr, addr, addrlen));
		n = tracker_udp_announce_reply_read(tracker->buffer, bytes, tracker->addr.ss_family==AF_INET6 ? 1 : 0, &tracker->transaction_id, &reply.interval, &reply.leechers, &reply.senders, &reply.peers);
		if (n < 0)
			code = n;
		else
			reply.peer_count = n;
	}
	
	tracker->onquery(tracker->param, code, &reply);
	if (reply.peers)
	{
		free(reply.peers);
		reply.peers = NULL;
		reply.peer_count = 0;
	}
}

static int tracker_udp_announce(struct tracker_t* tracker)
{
	int r;
	// build announce request
	r = tracker_udp_announce_write(tracker->buffer, sizeof(tracker->buffer), tracker->connection_id, tracker->transaction_id, tracker->info_hash, tracker->peer_id, tracker->downloaded, tracker->left, tracker->uploaded, tracker->event, tracker->port, tracker->path);
	assert(r > 0 && r < sizeof(tracker->buffer));

	if (r != socket_sendto(tracker->udp, tracker->buffer, r, 0, (struct sockaddr*)&tracker->addr, tracker->addrlen))
		return -1;

	return aio_recvfrom(&tracker->recv, tracker->timeout, tracker->aio, tracker->buffer, sizeof(tracker->buffer), tracker_udp_onannounce, tracker);
}

static void tracker_udp_onconnect(void* param, int code, size_t bytes, const struct sockaddr* addr, socklen_t addrlen)
{
	struct tracker_t* tracker;
	tracker = (struct tracker_t*)param;

	if (0 == code)
	{
		assert(addrlen == tracker->addrlen && 0 == memcmp(&tracker->addr, addr, addrlen));
		code = tracker_udp_connect_reply_read(tracker->buffer, bytes, &tracker->transaction_id, &tracker->connection_id);
	}

	if (0 == code)
	{
		code = tracker_udp_announce(tracker);
	}

	if (0 != code)
		tracker->onquery(tracker->param, code, NULL);
}

static int tracker_udp_connect(struct tracker_t* tracker)
{
	int r;
	// build connect request
	r = tracker_udp_connect_write(tracker->buffer, tracker->transaction_id);
	assert(r > 0 && r < sizeof(tracker->buffer));

	if (r != socket_sendto(tracker->udp, tracker->buffer, r, 0, (struct sockaddr*)&tracker->addr, tracker->addrlen))
		return r;

	return aio_recvfrom(&tracker->recv, tracker->timeout, tracker->aio, tracker->buffer, sizeof(tracker->buffer), tracker_udp_onconnect, tracker);
}

int tracker_udp(tracker_t* tracker)
{
	if (AF_INET != tracker->addr.ss_family && AF_INET6 != tracker->addr.ss_family)
		return -1;

	return tracker_udp_connect(tracker);
}
