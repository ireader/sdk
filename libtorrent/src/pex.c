// http://www.bittorrent.org/beps/bep_0011.html (Peer Exchange (PEX))
/*
{
	added: <one or more contacts in IPv4 compact format (string)>
	added.f: <optional, bit-flags, 1 byte per added IPv4 peer (string)>
	added6: <one or more contacts IPv6 compact format (string)>,
	added6.f: <optional, bit-flags, 1 byte per added IPv6 peer (string)>,
	dropped: <one or more contacts in IPv6 compact format (string)>,
	dropped6: <one or more contacts in IPv6 compact format (string)>
}
*/

#include "peer-extended.h"
#include "peer-message.h"
#include "byte-order.h"
#include "sys/sock.h"
#include "bencode.h"

static int bt_ipv4_read(const struct bvalue_t* peers, struct sockaddr_storage** addrs, size_t count)
{
	size_t i, n;
	void* ptr;
	const uint8_t* p;

	assert(0 == peers->v.str.bytes % 6);
	p = (const uint8_t*)peers->v.str.value;
	n = peers->v.str.bytes / 6;

	// It is common now to use a compact format where each peer is represented 
	// using only 6 bytes. The first 4 bytes contain the 32-bit ipv4 address. 
	// The remaining two bytes contain the port number. 
	// Both address and port use network-byte order.
	ptr = realloc(*addrs, (count + n) * sizeof(struct sockaddr_storage));
	if (NULL == ptr)
		return 0;

	*addrs = (struct sockaddr_storage*)ptr;
	for(i = 0; i < n; i++)
	{
		struct sockaddr_in* addr;
		addr = (struct sockaddr_in*)(*addrs + count + i);
		addr->sin_family = AF_INET;
		memcpy(&addr->sin_addr.s_addr, p, 4);
		memcpy(&addr->sin_port, p + 4, 2);
		p += 6;
	}

	return n;
}

static uint8_t* bt_ipv4_write(uint8_t* ptr, const struct sockaddr_storage* addrs, size_t count)
{
	size_t i;
	for (i = 0; i < count; i++)
	{
		if (AF_INET == addrs[i].ss_family)
		{
			struct sockaddr_in* addr;
			addr = (struct sockaddr_in*)(addrs + i);
			memcpy(ptr, &addr->sin_addr.s_addr, 4);
			memcpy(ptr + 4, &addr->sin_port, 2);
			ptr += 6;
		}
	}

	return ptr;
}

static int bt_ipv6_read(const struct bvalue_t* peers, struct sockaddr_storage** addrs, size_t count)
{
	size_t i, n;
	void* ptr;
	const uint8_t* p;
	
	assert(0 == peers->v.str.bytes % 18);
	p = (const uint8_t*)peers->v.str.value;
	n = peers->v.str.bytes / 18;

	// It is common now to use a compact format where each peer is represented 
	// using only 6 bytes. The first 4 bytes contain the 32-bit ipv4 address. 
	// The remaining two bytes contain the port number. 
	// Both address and port use network-byte order.
	ptr = realloc(*addrs, (count + n) * sizeof(struct sockaddr_storage));
	if (NULL == ptr)
		return 0;

	*addrs = (struct sockaddr_storage*)ptr;
	for (i = 0; i < n; i++)
	{
		struct sockaddr_in6* addr;
		addr = (struct sockaddr_in6*)(*addrs + count + i);
		addr->sin6_family = AF_INET6;
		memcpy(&addr->sin6_addr.s6_addr, p, 16);
		memcpy(&addr->sin6_port, p + 16, 2);
		p += 18;
	}

	return n;
}

static uint8_t* bt_ipv6_write(uint8_t* ptr, const struct sockaddr_storage* addrs, size_t count)
{
	size_t i;
	for (i = 0; i < count; i++)
	{
		if (AF_INET6 == addrs[i].ss_family)
		{
			struct sockaddr_in6* addr;
			addr = (struct sockaddr_in6*)(addrs + i);
			memcpy(ptr, &addr->sin6_addr.s6_addr, 16);
			memcpy(ptr + 16, &addr->sin6_port, 2);
			ptr += 18;
		}
	}

	return ptr;
}

static int bt_flags_read(const struct bvalue_t* v, uint8_t** flags, size_t count)
{
	size_t i, n;
	void* ptr;

	n = v->v.str.bytes;
	ptr = realloc(*flags, (count + n) * sizeof(struct sockaddr_storage));
	if (NULL == ptr)
		return 0;

	*flags = (uint8_t*)ptr;
	for (i = 0; i < n; i++)
	{
		(*flags)[count + i] = v->v.str.value[i];
	}

	return n;
}

int peer_pex_read(const uint8_t* buffer, int bytes, struct peer_pex_t* pex)
{
	int r;
	size_t i;
	struct bvalue_t root;
	r = bencode_read(buffer, bytes, &root);
	if (r <= 0)
		return r;

	memset(pex, 0, sizeof(*pex));
	if (root.type == BT_DICT)
	{
		for (i = 0; i < root.v.dict.count; i++)
		{
			if (0 == strcmp("added", root.v.dict.names[i].name))
			{
				assert(BT_STRING == root.v.dict.values[i].type);
				if (BT_STRING == root.v.dict.values[i].type)
				{
					pex->n_added += bt_ipv4_read(&root.v.dict.values[i], &pex->added, pex->n_added);
				}
			}
			else if (0 == strcmp("added.f", root.v.dict.names[i].name))
			{
				assert(BT_STRING == root.v.dict.values[i].type);
				if (BT_STRING == root.v.dict.values[i].type)
				{
					pex->n_flags += bt_flags_read(&root.v.dict.values[i], &pex->flags, pex->n_flags);
				}
			}
			else if (0 == strcmp("added6", root.v.dict.names[i].name))
			{
				assert(BT_STRING == root.v.dict.values[i].type);
				if (BT_STRING == root.v.dict.values[i].type)
				{
					pex->n_added += bt_ipv6_read(&root.v.dict.values[i], &pex->added, pex->n_added);
				}
			}
			else if (0 == strcmp("added6.f", root.v.dict.names[i].name))
			{
				assert(BT_STRING == root.v.dict.values[i].type);
				if (BT_STRING == root.v.dict.values[i].type)
				{
					pex->n_flags += bt_flags_read(&root.v.dict.values[i], &pex->flags, pex->n_flags);
				}
			}
			else if (0 == strcmp("dropped", root.v.dict.names[i].name))
			{
				assert(BT_STRING == root.v.dict.values[i].type);
				if (BT_STRING == root.v.dict.values[i].type)
				{
					pex->n_dropped += bt_ipv4_read(&root.v.dict.values[i], &pex->dropped, pex->n_dropped);
				}
			}
			else if (0 == strcmp("dropped6", root.v.dict.names[i].name))
			{
				assert(BT_STRING == root.v.dict.values[i].type);
				if (BT_STRING == root.v.dict.values[i].type)
				{
					pex->n_dropped += bt_ipv6_read(&root.v.dict.values[i], &pex->dropped, pex->n_dropped);
				}
			}
			else
			{
				// unknown keyword
				assert(0);
			}
		}
	}

	assert(pex->n_flags == pex->n_added);
	bencode_free(&root);
	return 0;
}

int peer_pex_write(uint8_t* buffer, int bytes, const struct peer_pex_t* pex)
{
	size_t i, n;
	uint8_t* p, *pend;
	buffer[4] = BT_EXTENDED;
	buffer[5] = BT_EXTENDED_PEX;
	buffer[6] = 'd';

	n = 7;
	p = malloc(18 * (pex->n_added > pex->n_dropped ? pex->n_added : pex->n_dropped));
	if (NULL == p)
		return -1; // no memory

	// added + added.f
	pend = bt_ipv4_write(p, pex->added, pex->n_added);
	if (pend > p)
	{
		n += snprintf((char*)buffer + n, bytes - n, "5:added%u:", (unsigned int)(pend - p));
		memcpy(buffer + n, p, pend - p);
		n += pend - p;

		pend = p;
		for (i = 0; i < pex->n_added; i++)
		{
			if (AF_INET == pex->added[i].ss_family)
				*pend++ = pex->flags[i];
		}

		n += snprintf((char*)buffer + n, bytes - n, "7:added.f%u:", (unsigned int)(pend - p));
		memcpy(buffer + n, p, pend - p);
		n += pend - p;
	}

	// added6 + added6.f
	pend = bt_ipv6_write(p, pex->added, pex->n_added);
	if (pend > p)
	{
		n += snprintf((char*)buffer + n, bytes - n, "6:added6%u:", (unsigned int)(pend - p));
		memcpy(buffer + n, p, pend - p);
		n += pend - p;

		pend = p;
		for (i = 0; i < pex->n_added; i++)
		{
			if (AF_INET6 == pex->added[i].ss_family)
				*pend++ = pex->flags[i];
		}

		n += snprintf((char*)buffer + n, bytes - n, "8:added6.f%u:", (unsigned int)(pend - p));
		memcpy(buffer + n, p, pend - p);
		n += pend - p;
	}

	// dropped
	pend = bt_ipv4_write(p, pex->dropped, pex->n_dropped);
	if (pend > p)
	{
		n += snprintf((char*)buffer + n, bytes - n, "7:dropped%u:", (unsigned int)(pend - p));
		memcpy(buffer + n, p, pend - p);
		n += pend - p;
	}

	// dropped6
	pend = bt_ipv6_write(p, pex->added, pex->n_added);
	if (pend > p)
	{
		n += snprintf((char*)buffer + n, bytes - n, "8:dropped6%u:", (unsigned int)(pend - p));
		memcpy(buffer + n, p, pend - p);
		n += pend - p;
	}

	free(p);
	buffer[n++] = 'e';
	nbo_w32(buffer, n - 4);
	return n;
}

#if defined(_DEBUG) || defined(DEBUG)
void peer_pex_read_test(void)
{
	size_t i;
	u_short port;
	char ip[SOCKET_ADDRLEN];
	struct peer_pex_t pex;
	const uint8_t data[] = { 0x64, 0x35, 0x3a, 0x61,
		0x64, 0x64, 0x65, 0x64, 0x31, 0x32, 0x30, 0x3a, 0x71, 0x59, 0x61, 0x72, 0x2d, 0x57, 0x6a, 0x53,
		0x74, 0xd0, 0x3c, 0xbe, 0x8d, 0xa8, 0xda, 0x01, 0xee, 0x69, 0x32, 0x62, 0x99, 0x08, 0xf8, 0x41,
		0x01, 0xeb, 0x3c, 0x2e, 0xa6, 0x37, 0xde, 0x7d, 0x69, 0xbf, 0x34, 0x82, 0x3b, 0x18, 0xd7, 0xbf,
		0x40, 0x16, 0x01, 0xf9, 0xf5, 0x86, 0x2e, 0xd7, 0x74, 0x21, 0x53, 0xa2, 0xc8, 0xd5, 0x01, 0x0b,
		0x67, 0x44, 0xcb, 0x9d, 0x01, 0xfe, 0xb0, 0x14, 0xe4, 0xd4, 0x31, 0x41, 0xcf, 0x84, 0xf0, 0x68,
		0x3b, 0x04, 0x8b, 0x67, 0x3c, 0xb8, 0x3b, 0x10, 0xea, 0xdc, 0x98, 0x82, 0x6d, 0x5d, 0x7d, 0x81,
		0x47, 0xb4, 0x6e, 0x0d, 0x37, 0x04, 0xf8, 0x85, 0x71, 0x3b, 0xa8, 0x7b, 0x93, 0x0a, 0x74, 0x18,
		0x62, 0x89, 0x35, 0x37, 0x75, 0x51, 0xcb, 0x0c, 0x39, 0x99, 0x76, 0x25, 0xb3, 0x4d, 0xcb, 0x88,
		0x37, 0x3a, 0x61, 0x64, 0x64, 0x65, 0x64, 0x2e, 0x66, 0x32, 0x30, 0x3a, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x37, 0x3a, 0x64, 0x72, 0x6f, 0x70, 0x70, 0x65, 0x64, 0x30, 0x3a, 0x65 };
	
	memset(&pex, 0, sizeof(pex));
	assert(0 == peer_pex_read(data, sizeof(data), &pex));

	printf("Added:\n");
	for (i = 0; i < pex.n_added; i++)
	{
		socket_addr_to((struct sockaddr*)&pex.added[i], AF_INET==pex.added[i].ss_family?sizeof(struct sockaddr_in):sizeof(struct sockaddr_in6), ip, &port);
		printf("%s:%u\n", ip, port);
	}

	printf("\nDropped:\n");
	for (i = 0; i < pex.n_dropped; i++)
	{
		socket_addr_to((struct sockaddr*)&pex.dropped[i], AF_INET == pex.dropped[i].ss_family ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6), ip, &port);
		printf("%s:%u\n", ip, port);
	}

	printf("\nFlags:\n");
	for (i = 0; i < pex.n_flags; i++)
	{
		printf(" 0x%X", (unsigned int)pex.flags[i]);
	}
}
#endif