// http://www.bittorrent.org/beps/bep_0003.html
// http://www.bittorrent.org/beps/bep_0023.html
// http://www.bittorrent.org/beps/bep_0015.html
// http://www.bittorrent.org/beps/bep_0041.html

#include "tracker.h"
#include "bencode.h"
#include "uri-parse.h"
#include "http-client.h"
#include "urlcodec.h"
#include <inttypes.h>
#include <string.h>
#include <stdio.h>

#if defined(OS_WINDOWS)
#define strcasecmp _stricmp
#endif

struct tracker_contex_t
{
	http_client_t* http;
	struct tracker_t* tracker;
};

int hash_sha1(const uint8_t* data, size_t bytes, uint8_t sha1[20]);

static int tracker_read_peer(struct tracker_t* tracker, const struct bvalue_t* peers)
{
	int r;
	size_t i, j;
	
	if (BT_LIST == peers->type)
	{
		char* ip;
		int32_t port;
		const char* name;

		assert(!tracker->peers && !tracker->peer_count);
		tracker->peers = malloc(sizeof(struct sockaddr_storage) * peers->v.list.count);
		if (!tracker->peers)
			return -1;
		tracker->peer_count = 0;

		for (i = r = 0; i < peers->v.list.count && 0 == r; i++)
		{
			const struct bvalue_t* peer;
			peer = peers->v.list.values + i;
			if (BT_DICT != peer->type)
			{
				assert(0);
				continue;
			}

			ip = NULL;
			port = 0;
			for (j = 0; j < peer->v.dict.count && 0 == r; j++)
			{
				const struct bvalue_t* node;
				name = peer->v.dict.names[j].name;
				node = peer->v.dict.values + j;

				// This tracker returns a list of peers that are currently transferring the file. 
				// The list of peers is implemented as a list of bencoded dicts. 
				// Each dict in the list contains three fields: peer id, ip, and port. 
				// The peer id is 20 bytes plus 3 bytes bencoding overhead. 
				// The ip is a string containing a domain name or an IP address, 
				// and an integer port number. 
				// The ip is variable length.
				if (0 == strcmp("peer id", name))
				{
					// ignore
					assert(BT_STRING == node->type && 20 == node->v.str.bytes);
				}
				else if (0 == strcmp("ip", name))
				{
					r = bencode_get_string(node, &ip);
				}
				else if (0 == strcmp("port", name))
				{
					r = bencode_get_int(node, &port);
				}
				else
				{
					assert(0); // unknown field
				}
			}

			if (0 == r && ip && port > 0)
			{
				socklen_t addrlen;
				addrlen = sizeof(struct sockaddr_storage);
				if (0 == socket_addr_from(&tracker->peers[tracker->peer_count], &addrlen, ip, (u_short)port))
					++tracker->peer_count;
			}

			if (ip)
				free(ip);
		}
	}
	else if (BT_STRING == peers->type)
	{
		const uint8_t* p;
		p = (const uint8_t*)peers->v.str.value;

		// It is common now to use a compact format where each peer is represented 
		// using only 6 bytes. The first 4 bytes contain the 32-bit ipv4 address. 
		// The remaining two bytes contain the port number. 
		// Both address and port use network-byte order.
		if (0 == peers->v.str.bytes % 6)
		{
			// IPv4
			assert(!tracker->peers && !tracker->peer_count);
			tracker->peers = malloc(sizeof(struct sockaddr_storage) * (peers->v.str.bytes / 6));
			if (!tracker->peers)
				return -1;
			tracker->peer_count = peers->v.str.bytes / 6;
			for (i = 0; i < tracker->peer_count; i++)
			{
				struct sockaddr_in* addr;
				addr = (struct sockaddr_in*)&tracker->peers[i];
				memset(addr, 0, sizeof(*addr));
				addr->sin_family = AF_INET;
				memcpy(&addr->sin_addr.s_addr, p, 4);
				memcpy(&addr->sin_port, p + 4, 2);
				p += 6;
			}
		}
		// never come here
		else if (0 == peers->v.str.bytes % 18)
		{
			// IPv6
			assert(!tracker->peers && !tracker->peer_count);
			tracker->peers = malloc(sizeof(struct sockaddr_storage) * (peers->v.str.bytes / 18));
			if (!tracker->peers)
				return -1;
			tracker->peer_count = peers->v.str.bytes / 18;
			for (i = 0; i < tracker->peer_count; i++)
			{
				struct sockaddr_in6* addr;
				addr = (struct sockaddr_in6*)&tracker->peers[i];
				memset(addr, 0, sizeof(*addr));
				addr->sin6_family = AF_INET6;
				memcpy(&addr->sin6_addr.s6_addr, p, 16);
				memcpy(&addr->sin6_port, p + 16, 2);
				p += 18;
			}
		}
		else
		{
			assert(0); // invalid address list
			return -1;
		}
	}
	else
	{
		assert(0);
		return -1;
	}

	return 0;
}

static int tracker_read(struct tracker_t* tracker, const struct bvalue_t* root)
{
	int r;
	size_t i;
	const char* name;
	const struct bvalue_t* node;

	if (BT_DICT != root->type)
		return -1;

	for (i = r = 0; i < root->v.dict.count && 0 == r; i++)
	{
		name = root->v.dict.names[i].name;
		node = root->v.dict.values + i;

		if (0 == strcmp("complete", name))
		{
			r = bencode_get_int(node, &tracker->seeders);
		}
		else if (0 == strcmp("incomplete", name))
		{
			r = bencode_get_int(node, &tracker->leechers);
		}
		else if (0 == strcmp("interval", name))
		{
			r = bencode_get_int(node, &tracker->interval);
		}
		else if (0 == strcmp("min interval", name))
		{
			r = bencode_get_int(node, &tracker->interval);
		}
		else if (0 == strcmp("peers", name))
		{
			r = tracker_read_peer(tracker, node);
		}
		else
		{
			assert(0); // unknown keyword
		}
	}

	return r;
}

static int tracker_user(const char* usr, uint8_t id[20])
{
	static const char* s_agent = "-XL0012-";
	hash_sha1((const uint8_t*)usr, strlen(usr), id);
	memcpy(id, s_agent, strlen(s_agent));
	return 0;
}

// /announce?info_hash=O.%FD%D1%05%C3%D9%7C%B6%3F%3B%B9%EC%E9i%5E%C3~e%E4
//			&peer_id=-XL0012-%04%01b%FA%7B%E1%FBZ1j%E0%A3
//			&ip=192.168.3.105
//			&port=15000
//			&uploaded=0
//			&downloaded=0
//			&left=5866950861
//			&numwant=200
//			&key=5089
//			&compact=1
//			&event=started
static int tracker_uri(char* url, size_t bytes,
	const char* path, 
	const uint8_t info_hash[20], 
	const uint8_t peer_id[20],
	int port,
	uint64_t uploaded,
	uint64_t downloaded,
	uint64_t left,
	int numwant,
	enum tracker_event_t event)
{
	static const char* s_event[] = { "none", "completed", "started", "stopped" };
	char info_hash_encode[20 * 3 + 1];
	char peer_id_encode[20 * 3 + 1];
	
	url_encode((const char*)info_hash, 20, info_hash_encode, sizeof(info_hash_encode));
	url_encode((const char*)peer_id, 20, peer_id_encode, sizeof(peer_id_encode));
	return snprintf(url, bytes, "%s?info_hash=%s&peer_id=%s&port=%d&uploaded=%" PRIu64 "&downloaded=%" PRIu64 "&left=%" PRIu64 "numwant=%d&compact=1&event=%s",
					path,
					info_hash_encode, peer_id_encode, port,
					uploaded, downloaded, left,
					numwant, s_event[event % 4]);
}

static void http_onget(void *param, int code)
{
	void* reply;
	size_t bytes;
	struct bvalue_t root;
	struct tracker_contex_t* t;
	t = (struct tracker_contex_t*)param;

	if (0 == code && 0 == http_client_get_content(t->http, &reply, &bytes))
	{
		code = bencode_read((const uint8_t*)reply, bytes, &root);
		if (0 == code)
		{
			code = tracker_read(t->tracker, &root);
			bencode_free(&root);
		}
	}

	if(0 != code)
	{
		char errmsg[64];
		snprintf(errmsg, sizeof(errmsg) - 1, "HTTP error: %d", code);
		t->tracker->errmsg = strdup(errmsg);
	}
}

static int tracker_http(const struct uri_t* uri, 
	const uint8_t info_hash[20],
	const uint8_t peer_id[20],
	int port,
	uint64_t downloaded,
	uint64_t left,
	uint64_t uploaded,
	enum tracker_event_t event,
	struct tracker_t* tracker)
{
	int r;
	char path[256];
	
	struct tracker_contex_t t;
	struct http_header_t headers[4];
	
	headers[0].name = "User-Agent";
	headers[0].value = "Bittorrent";
	headers[1].name = "Accept-Language";
	headers[1].value = "en-US,en;q=0.5";
	headers[2].name = "Accept";
	headers[2].value = "*/*";
	headers[3].name = "Connection";
	headers[3].value = "Close";

	r = tracker_uri(path, sizeof(path), uri->path, info_hash, peer_id, port, uploaded, downloaded, left, 200, event);

	t.tracker = tracker;
	t.http = http_client_create(uri->host, (unsigned short)(uri->port ? uri->port : 80), 1);
	r = http_client_get(t.http, path, headers, sizeof(headers) / sizeof(headers[0]), http_onget, &t);
	http_client_destroy(t.http);

	return r;
}


int tracker_udp(const struct uri_t* uri,
	const uint8_t info_hash[20],
	const uint8_t peer_id[20],
	int port,
	uint64_t downloaded,
	uint64_t left,
	uint64_t uploaded,
	enum tracker_event_t event,
	struct tracker_t* tracker);

int tracker_get(const char* url,
	const uint8_t info_hash[20],
	const char* usr,
	int port,
	uint64_t downloaded,
	uint64_t left,
	uint64_t uploaded,
	enum tracker_event_t event,
	struct tracker_t* tracker)
{
	int r;
	struct uri_t* uri;
	uint8_t peer_id[20];

	uri = uri_parse(url, strlen(url));
	if (!uri)
		return -1;

	tracker_user(usr, peer_id);

	if (uri->scheme && 0 == strcasecmp("udp", uri->scheme))
	{
		r = tracker_udp(uri, info_hash, peer_id, port, downloaded, left, uploaded, event, tracker);
	}
	else
	{
		r = tracker_http(uri, info_hash, peer_id, port, downloaded, left, uploaded, event, tracker);
	}

	uri_free(uri);
	return r;
}

int tracker_free(struct tracker_t* tracker)
{
	if (tracker->errmsg)
	{
		free(tracker->errmsg);
		tracker->errmsg = NULL;
	}

	if (tracker->peers)
	{
		free(tracker->peers);
		tracker->peers = NULL;
		tracker->peer_count = 0;
	}

	return 0;
}
