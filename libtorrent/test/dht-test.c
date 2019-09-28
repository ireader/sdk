#include "dht.h"
#include "magnet.h"
#include "torrent.h"
#include "sockutil.h"
#include "aio-worker.h"
#include "sys/system.h"
#include "sys/atomic.h"
#include <assert.h>
#include <stdio.h>

#define NODEFILE "dht.nodes"

static int dht_node_save(void* param, const uint8_t id[20], const struct sockaddr_storage* addr)
{
	FILE* fp = (FILE*)param;
	uint8_t family = (uint8_t)addr->ss_family;

	if (AF_INET == addr->ss_family)
	{
		struct sockaddr_in* ipv4;
		ipv4 = (struct sockaddr_in*)addr;
		fwrite(id, 1, 20, fp);
		fwrite(&family, 1, 1, fp);
		fwrite(&ipv4->sin_addr.s_addr, 1, 4, fp);
		fwrite(&ipv4->sin_port, 1, 2, fp);
	}
	else if(AF_INET6 == addr->ss_family)
	{
		struct sockaddr_in6* ipv6;
		ipv6 = (struct sockaddr_in6*)addr;
		fwrite(id, 1, 20, fp);
		fwrite(&family, 1, 1, fp);
		fwrite(&ipv6->sin6_addr.s6_addr, 1, 16, fp);
		fwrite(&ipv6->sin6_port, 1, 2, fp);
	}
	else
	{
		assert(0);
	}

	return 0;
}

static int dht_node_load(dht_t* dht, const char* file)
{
	uint8_t id[20];
	uint8_t buffer[64];
	struct sockaddr_storage addr;

	FILE* fp = fopen(file, "rb");
	
	while (21 == fread(buffer, 1, 21, fp))
	{
		memcpy(id, buffer, 20);
		memset(&addr, 0, sizeof(addr));
		addr.ss_family = buffer[20];
		if (AF_INET == addr.ss_family)
		{
			struct sockaddr_in* ipv4;
			ipv4 = (struct sockaddr_in*)&addr;
			if (6 != fread(buffer + 21, 1, 6, fp))
				break;
			memcpy(&ipv4->sin_addr.s_addr, buffer + 21, 4);
			memcpy(&ipv4->sin_port, buffer + 25, 2);
		}
		else if (AF_INET6 == addr.ss_family)
		{
			struct sockaddr_in6* ipv6;
			ipv6 = (struct sockaddr_in6*)&addr;
			if (18 != fread(buffer + 21, 1, 18, fp))
				break;
			memcpy(&ipv6->sin6_addr.s6_addr, buffer + 21, 18);
			memcpy(&ipv6->sin6_port, buffer + 37, 2);
		}
		else
		{
			assert(0);
		}

		dht_add_node(dht, id, &addr);
	}

	fclose(fp);
}

static void dht_save(dht_t* dht, const char* file)
{
	FILE* fp = fopen(file, "wb");
	dht_list_node(dht, dht_node_save, fp);
	fclose(fp);
}

static void dht_handle_ping(void* param, int code)
{
	dht_t* dht = *(dht_t**)param;
	dht_save(dht, NODEFILE);
}

static void dht_handle_find_node(void* param, int code, const uint8_t id[20])
{
	struct magnet_t* magnet;
	dht_t* dht = *(dht_t**)param;
	dht_save(dht, NODEFILE);

	magnet = magnet_parse("magnet:?xt=urn:btih:40948DD35268BDEA37894FCB5390402CC3AAD95E");
	dht_get_peers(dht, magnet->info_hash);
	magnet_free(magnet);
}

static void dht_handle_get_peers(void* param, int code, const uint8_t info_hash[20], const struct sockaddr_storage* peers, uint32_t count)
{
	dht_t* dht = *(dht_t**)param;
	dht_save(dht, NODEFILE);
}

void dht_test(void)
{
	int i;
	uint8_t id[20];
	dht_t* dht;
	socklen_t addrlen;
	struct sockaddr_storage addr;
	struct dht_handler_t handler;

	const struct
	{
		const char* host;
		uint16_t port;
		uint8_t id[20];
	} bootstrap[] = {
		{ "dht.libtorrent.org", 25401, {0xde, 0xc8, 0xae, 0x69, 0x73, 0x51, 0xff, 0x4a, 0xec, 0x29, 0xcd, 0xba, 0xab, 0xf2, 0xfb, 0xe3, 0x46, 0x7c, 0xc2, 0x67} },
		{ "router.bittorrent.com", 6881, { 0xeb, 0xff, 0x36, 0x69, 0x73, 0x51, 0xff, 0x4a, 0xec, 0x29, 0xcd, 0xba, 0xab, 0xf2, 0xfb, 0xe3, 0x46, 0x7c, 0xc2, 0x67 } },
		{ "dht.transmissionbt.com", 6881, { 0x3c, 0x00, 0x72, 0x73, 0x48, 0xb3, 0xb8, 0xed, 0x70, 0xba, 0xa1, 0xe1, 0x41, 0x1b, 0x38, 0x69, 0xd8, 0x48, 0x13, 0x21} },
		{ "router.utorrent.com", 6881, {0}},
		{ "dht.aelitis.com", 6881, { 0 } },
	};

	aio_worker_init(4);
	
	memset(&handler, 0, sizeof(handler));
	handler.ping = dht_handle_ping;
	handler.find_node = dht_handle_find_node;
	handler.get_peers = dht_handle_get_peers;

	torrent_peer_id("libtorrent", id);
	dht = dht_create(id, 6881, &handler, &dht);

	dht_node_load(dht, NODEFILE);
	for (i = 0; i < sizeof(bootstrap) / sizeof(bootstrap[0]); i++)
	{
		addrlen = sizeof(addr);
		socket_addr_from(&addr, &addrlen, bootstrap[i].host, bootstrap[i].port);
//		dht_ping(dht, &addr);
		dht_add_node(dht, bootstrap[i].id, &addr);
	}

	dht_find_node(dht, id);

	while (1)
	{
		system_sleep(2000);
	}

	dht_destroy(dht);
	aio_worker_clean(4);
}
