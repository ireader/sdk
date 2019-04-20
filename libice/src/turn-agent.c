#include "stun-internal.h"
#include "turn-internal.h"

struct turn_allocation_t* turn_agent_allocation_find_by_relay(struct list_head* root, const struct sockaddr_storage* relayed)
{
	struct list_head* pos;
	struct turn_allocation_t* allocate;
	list_for_each(pos, root)
	{
		allocate = list_entry(pos, struct turn_allocation_t, link);
		if (0 == turn_sockaddr_cmp(&allocate->addr.relay, relayed))
			return allocate;
	}
	return NULL;
}

struct turn_allocation_t* turn_agent_allocation_find_by_address(struct list_head* root, const struct sockaddr_storage* host, const struct sockaddr_storage* peer)
{
	struct list_head* pos;
	struct turn_allocation_t* allocate;
	list_for_each(pos, root)
	{
		allocate = list_entry(pos, struct turn_allocation_t, link);
		if (0 == turn_sockaddr_cmp(&allocate->addr.host, host) || 0 == turn_sockaddr_cmp(&allocate->addr.peer, peer))
			return allocate;
	}
	return NULL;
}

int turn_agent_allocation_insert(struct stun_agent_t* turn, struct turn_allocation_t* allocate)
{
	locker_lock(&turn->locker);
	assert(NULL == turn_agent_allocation_find_by_address(&turn->turnclients, &allocate->addr.host, &allocate->addr.peer));
	list_insert_after(&allocate->link, &turn->turnclients);
	locker_unlock(&turn->locker);
	return 0;
}

int turn_agent_allocation_remove(struct stun_agent_t* turn, struct turn_allocation_t* allocate)
{
	locker_lock(&turn->locker);
	list_remove(&allocate->link);
	locker_unlock(&turn->locker);
	return 0;
}
