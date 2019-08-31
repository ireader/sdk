#include "stun-internal.h"
#include "turn-internal.h"
#include "sys/system.h"
#include <stdlib.h>

struct turn_allocation_t* turn_allocation_create(void)
{
	struct turn_allocation_t* allocate;
	allocate = (struct turn_allocation_t*)calloc(1, sizeof(*allocate));
	if (allocate)
	{
		LIST_INIT_HEAD(&allocate->link);
	}
	return allocate;
}

int turn_allocation_destroy(struct turn_allocation_t** pp)
{
	struct turn_allocation_t* allocate;

	if (!pp || !*pp)
		return -1;

	allocate = *pp;
	free(allocate);
	*pp = NULL;
	return 0;
}

const struct turn_permission_t* turn_allocation_find_permission(const struct turn_allocation_t* allocate, const struct sockaddr* addr)
{
	int i;
	const struct turn_permission_t* p;
	for (i = 0; i < allocate->npermission; i++)
	{
		p = &allocate->permissions[i];
		if (0 == turn_sockaddr_cmp((const struct sockaddr*)&p->addr, addr))
			return p;
	}
	return NULL;
}

int turn_allocation_add_permission(struct turn_allocation_t* allocate, const struct sockaddr* addr)
{
	struct turn_permission_t permission, *p;
	p = (struct turn_permission_t*)turn_allocation_find_permission(allocate, addr);
	if (p)
	{
		p->expired = system_clock() + allocate->lifetime * 1000;
		return 0;
	}
	
	if (allocate->npermission + 1 > sizeof(allocate->permissions) / sizeof(allocate->permissions[0]))
	{
		assert(0);
		return -1;
	}

	memset(&permission, 0, sizeof(struct turn_permission_t));
	memcpy(&permission.addr, addr, socket_addr_len(addr));
	permission.expired = system_clock() + allocate->lifetime * 1000;
	memcpy(&allocate->permissions[allocate->npermission++], &permission, sizeof(struct turn_permission_t));
	return 0;
}

const struct turn_channel_t* turn_allocation_find_channel(const struct turn_allocation_t* allocate, uint16_t channel)
{
	int i;
	const struct turn_channel_t* p;
	for (i = 0; i < allocate->nchannel; i++)
	{
		p = &allocate->channels[i];
		if (p->channel == channel)
			return p;
	}
	return NULL;
}

const struct turn_channel_t* turn_allocation_find_channel_by_peer(const struct turn_allocation_t* allocate, const struct sockaddr* addr)
{
	int i;
	const struct turn_channel_t* p;
	for (i = 0; i < allocate->nchannel; i++)
	{
		p = &allocate->channels[i];
		if (0 == socket_addr_compare((const struct sockaddr*)&p->addr, addr))
			return p;
	}
	return NULL;
}

int turn_allocation_add_channel(struct turn_allocation_t* allocate, const struct sockaddr* addr, uint16_t channel)
{
    int r;
	struct turn_channel_t c, *p;
	const struct turn_channel_t *p2;
	p = (struct turn_channel_t*)turn_allocation_find_channel(allocate, channel);
	if (p)
	{
		// 1. The channel number is not currently bound to a different transport address
		// 2. The transport address is not currently bound to a different channel number
        r =turn_sockaddr_cmp((const struct sockaddr*)&p->addr, addr);
        p2 =turn_allocation_find_channel_by_peer(allocate, addr);
		if (0 != socket_addr_compare((const struct sockaddr*)&p->addr, addr) || p != turn_allocation_find_channel_by_peer(allocate, addr))
			return -1; // channel in-use

		p->expired = system_clock() + allocate->lifetime * 1000;
		return 0;
	}

	if (allocate->nchannel + 1 > sizeof(allocate->channels) / sizeof(allocate->channels[0]))
	{
		assert(0);
		return -1;
	}

	memset(&c, 0, sizeof(struct turn_channel_t));
	memcpy(&c.addr, addr, socket_addr_len(addr));
	c.expired = system_clock() + allocate->lifetime * 1000;
    c.channel = channel;
	memcpy(&allocate->channels[allocate->nchannel++], &c, sizeof(struct turn_channel_t));
	return 0;
}
