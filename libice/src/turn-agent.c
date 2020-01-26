#include "stun-internal.h"
#include "turn-internal.h"
#include "sys/system.h"

// for RESERVATION-TOKEN
struct turn_allocation_t* turn_agent_allocation_find_by_token(struct list_head* root, const void* token)
{
	struct list_head* pos;
	struct turn_allocation_t* allocate;
	list_for_each(pos, root)
	{
		allocate = list_entry(pos, struct turn_allocation_t, link);
		if (allocate == token)
			return allocate;
	}
	return NULL;
}

struct turn_allocation_t* turn_agent_allocation_find_by_relay(struct list_head* root, const struct sockaddr* relayed)
{
	struct list_head* pos;
	struct turn_allocation_t* allocate;
	list_for_each(pos, root)
	{
		allocate = list_entry(pos, struct turn_allocation_t, link);
		if (0 == socket_addr_compare((const struct sockaddr*)&allocate->addr.relay, relayed))
			return allocate;
	}
	return NULL;
}

struct turn_allocation_t* turn_agent_allocation_find_by_address(struct list_head* root, const struct sockaddr* host, const struct sockaddr* peer)
{
	struct list_head* pos;
	struct turn_allocation_t* allocate;
	list_for_each(pos, root)
	{
		allocate = list_entry(pos, struct turn_allocation_t, link);
		if (0 == socket_addr_compare((const struct sockaddr*)&allocate->addr.host, host) && 0 == socket_addr_compare((const struct sockaddr*)&allocate->addr.peer, peer))
			return allocate;
	}
	return NULL;
}

int turn_agent_allocation_insert(struct list_head* root, struct turn_allocation_t* allocate)
{
	assert(NULL == turn_agent_allocation_find_by_address(root, (const struct sockaddr*)&allocate->addr.host, (const struct sockaddr*)&allocate->addr.peer));
	list_insert_after(&allocate->link, root->prev);
	return 0;
}

int turn_agent_allocation_remove(struct list_head* root, struct turn_allocation_t* allocate)
{
    (void)root;
	list_remove(&allocate->link);
	return 0;
}

struct turn_allocation_t* turn_agent_allocation_reservation_token(struct stun_agent_t* turn, struct turn_allocation_t* from)
{
	struct turn_allocation_t* next;
	next = turn_allocation_create();
	if (!next)
		return NULL;

	next->expire = from->expire;
	next->dontfragment = from->dontfragment;
	next->peertransport = from->peertransport;
	memcpy(&next->addr, &from->addr, sizeof(next->addr));
	memcpy(&next->auth, &from->auth, sizeof(next->auth));
	// next higher port
	if (AF_INET == from->addr.relay.ss_family)
		((struct sockaddr_in*)&next->addr.relay)->sin_port = htons(ntohs(((struct sockaddr_in*)&from->addr.relay)->sin_port) + 1);
	if (AF_INET6 == from->addr.relay.ss_family)
		((struct sockaddr_in6*)&next->addr.relay)->sin6_port = htons(ntohs(((struct sockaddr_in6*)&from->addr.relay)->sin6_port) + 1);

	// save to reservation token
    turn_agent_allocation_insert(&turn->turnreserved, next);
    assert(sizeof(intptr_t) <= sizeof(from->token));
	*(intptr_t*)from->token = (intptr_t)next;
    return next;
}

int turn_agent_allocation_cleanup(struct stun_agent_t* turn)
{
    uint64_t now;
    struct turn_allocation_t* allocate;
    struct list_head* pos, *next;
    
    now = system_clock();
    
    list_for_each_safe(pos, next, &turn->turnclients)
    {
        allocate = list_entry(pos, struct turn_allocation_t, link);
        // TODO: check permission/channel expire

        if(allocate->expire < now)
            continue;
        
        list_remove(pos);
        turn_allocation_destroy(&allocate);
    }
    
    list_for_each_safe(pos, next, &turn->turnservers)
    {
        allocate = list_entry(pos, struct turn_allocation_t, link);
        // TODO: check permission/channel expire
        
        if(allocate->expire < now)
            continue;
        
        list_remove(pos);
        turn_allocation_destroy(&allocate);
    }
    
    list_for_each_safe(pos, next, &turn->turnreserved)
    {
        allocate = list_entry(pos, struct turn_allocation_t, link);
        // TODO: check permission/channel expire
        
        if(allocate->expire < now)
            continue;
        
        list_remove(pos);
        turn_allocation_destroy(&allocate);
    }

    return 0;
}
