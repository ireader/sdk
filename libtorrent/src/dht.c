// http://www.bittorrent.org/beps/bep_0005.html (DHT Protocol)

#include "dht.h"
#include "dht-message.h"
#include "udp-socket.h"
#include "sys/locker.h"
#include "sys/thread.h"
#include "sys/atomic.h"
#include "sys/system.h"
#include "sockutil.h"
#include "app-log.h"
#include "router.h"
#include "heap.h"
#include "list.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define N_TASK 8
#define N_TIMEOUT 30000 // 30s
#define N_NOTIFY 3

#define KEEP_ALIVE_TASK_ID 0

#define ADDR_LEN(addr) (AF_INET6 == (addr)->ss_family ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in))

struct dht_bootstrap_t
{
	const char* id;
	const char* host;
	uint16_t port;	
};

static const struct dht_bootstrap_t s_bootstrap[] = {
	{ "dec8ae697351ff4aec29cdbaabf2fbe3467cc267", "dht.libtorrent.org", 25401 },
	{ "3c00727348b3b8ed70baa1e1411b3869d8481321", "dht.transmissionbt.com", 6881 },
	{ "ebff36697351ff4aec29cdbaabf2fbe3467cc267", "router.bittorrent.com", 6881 },
};

struct dht_t
{
	uint8_t id[20];
	int32_t transaction_id;
	uint64_t clock;

	socket_t notify;
	struct udp_socket_t udp;
	socklen_t addrlen;
	struct sockaddr_storage addr;

	uint8_t buffer[2 * 1024 * 1024];
	
	struct dht_handler_t handler;
	void* param;

	locker_t locker;
	pthread_t thread;
	volatile int running;
	struct router_t* router;
	struct list_head tasks, tasks2;
};

struct dht_io_t
{
	struct dht_t* dht;
	struct sockaddr_storage addr;
	socklen_t addrlen;
};

struct dht_task_t
{
	struct list_head link;
	uint16_t transaction;
	uint8_t type;
};

struct dht_ping_task_t
{
	struct dht_task_t task;
	struct sockaddr_storage addr;

	uint8_t retry;
	uint64_t clock;	
};

struct dht_find_task_t
{
	struct dht_task_t task;
	heap_t* heap;
	uint8_t id[20];

	size_t peers;
};

struct dht_keepalive_task_t
{
	struct dht_task_t task;
	struct node_t* nodes[50];
	unsigned int count;
};

static uint16_t dht_transaction_id(struct dht_t* dht);
static void dht_find_task_free(struct dht_find_task_t* task);
static int STDCALL dht_process(void* param);
static int dht_bind(struct dht_t* dht, uint16_t port);
static int dht_unbind(struct dht_t* dht);
static int dht_notify(struct dht_t* dht);
static int dht_schedule(struct dht_t* dht);
static int dht_schedule_find_node(struct dht_t* dht, struct dht_find_task_t* task);
static int dht_schedule_get_peers(struct dht_t* dht, struct dht_find_task_t* task);

dht_t* dht_create(const uint8_t id[20], uint16_t port, struct dht_handler_t* handler, void* param)
{
	dht_t* dht;
	dht = (dht_t*)calloc(1, sizeof(*dht));
	if (!dht) return NULL;

	if (0 != dht_bind(dht, port))
	{
		free(dht);
		return NULL;
	}

	locker_create(&dht->locker);
	LIST_INIT_HEAD(&dht->tasks);
	LIST_INIT_HEAD(&dht->tasks2);
	dht->router = router_create(id);
	memcpy(dht->id, id, sizeof(dht->id));
	memcpy(&dht->handler, handler, sizeof(dht->handler));
	dht->param = param;

	dht->running = 1;
	thread_create(&dht->thread, dht_process, dht);
	return dht;
}

int dht_destroy(dht_t* dht)
{
	struct list_head* pos, *next;
	struct dht_task_t* task;

	dht->running = 0;
	thread_destroy(dht->thread);

	list_for_each_safe(pos, next, &dht->tasks2)
	{
		list_insert_after(pos, dht->tasks.prev);
	}

	list_for_each_safe(pos, next, &dht->tasks)
	{
		task = list_entry(pos, struct dht_task_t, link);
		if (DHT_FIND_NODE == task->type || DHT_GET_PEERS == task->type)
		{
			dht_find_task_free((struct dht_find_task_t*)task);
		}
		else
		{
			free(task);
		}
	}

	dht_unbind(dht);

	if (dht->router)
	{
		router_destroy(dht->router);
		dht->router = NULL;
	}

	locker_destroy(&dht->locker);
	free(dht);
	return 0;
}

static void dht_find_task_free(struct dht_find_task_t* task)
{
	struct node_t* node;
	while (!heap_empty(task->heap))
	{
		node = (struct node_t*)heap_top(task->heap);
		node_release(node);
		heap_pop(task->heap);
	}
	free(task);
}

static int dht_bind(dht_t* dht, uint16_t port)
{
	if (0 != udp_socket_create(port, &dht->udp))
		return -1;

	dht->notify = socket_invalid;
	dht->addrlen = sizeof(dht->addr);
	if (socket_invalid != dht->udp.udp)
	{
		dht->notify = socket(AF_INET, SOCK_DGRAM, 0);
		dht->addrlen = sizeof(struct sockaddr_in);
		socket_addr_from_ipv4((struct sockaddr_in*)&dht->addr, "localhost", dht->udp.port);		
	}
	else if(socket_invalid != dht->udp.udp6)
	{
		dht->notify = socket(AF_INET6, SOCK_DGRAM, 0);
		dht->addrlen = sizeof(struct sockaddr_in6);
		socket_addr_from_ipv6((struct sockaddr_in6*)&dht->addr, "localhost", dht->udp.port);		
	}
	else
	{
		assert(0); // impossible
	}

	return 0;
}

static int dht_unbind(dht_t* dht)
{
	udp_socket_destroy(&dht->udp);

	if (socket_invalid != dht->notify)
	{
		socket_close(dht->notify);
		dht->notify = socket_invalid;
	}
	return 0;
}

static int dht_notify(dht_t* dht)
{
	int r;
	const uint8_t buf[N_NOTIFY] = { 0xAB, 0xCD, 0xEF };
	assert(dht->addrlen == sizeof(struct sockaddr_in) || dht->addrlen == sizeof(struct sockaddr_in6));
	r = socket_sendto(dht->notify, buf, sizeof(buf), 0, (const struct sockaddr*)&dht->addr, dht->addrlen);
	assert(N_NOTIFY == r);
	return r > 0 ? 0 : -1;
}

int dht_add_node(dht_t* dht, const uint8_t id[20], const struct sockaddr_storage* addr)
{
	return router_add(dht->router, id, addr, NULL);
}

struct dht_list_node_t
{
	int(*onnode)(void* param, const uint8_t id[20], const struct sockaddr_storage* addr);
	void* param;
};
static int dht_onnode(void* param, const struct node_t* node)
{
	struct dht_list_node_t* callback = (struct dht_list_node_t*)param;
	return callback->onnode(callback->param, node->id, &node->addr);
}
int dht_list_node(dht_t* dht, int(*onnode)(void* param, const uint8_t id[20], const struct sockaddr_storage* addr), void* param)
{
	struct dht_list_node_t callback;
	callback.onnode = onnode;
	callback.param = param;
	return router_list(dht->router, dht_onnode, &callback);
}

int dht_ping(dht_t* dht, const struct sockaddr_storage* addr)
{
	struct dht_ping_task_t* task;
	task = malloc(sizeof(*task));
	if (!task) return ENOMEM;

	memcpy(&task->addr, addr, sizeof(task->addr));
	task->task.transaction = 0; // placeholder
	task->task.type = DHT_PING;
	task->clock = 0;
	task->retry = 0;

	locker_lock(&dht->locker);
	list_insert_after(&task->task.link, dht->tasks2.prev);
	locker_unlock(&dht->locker);
	return dht_notify(dht);
}

int dht_find_node(dht_t* dht, const uint8_t id[20])
{
	int i, r;
	struct node_t* nodes[N_TASK];
	struct dht_find_task_t* task;
	
	task = calloc(1, sizeof(*task));
	if (!task) return ENOMEM;

	r = router_nearest(dht->router, id, nodes, sizeof(nodes) / sizeof(nodes[0]));
	if (0 == r)
	{
		free(task);
		return ENOENT; // don't have any node, add bootstrap node first
	}

	memcpy(&task->id, id, sizeof(task->id));
	task->task.transaction = 0; // placeholder
	task->task.type = DHT_FIND_NODE;

	task->heap = heap_create(node_compare_great, task->id);
	heap_reserve(task->heap, sizeof(nodes) / sizeof(nodes[0]) + 1);
	for (i = 0; i < r; i++)
	{
		nodes[i]->retry = 0;
		nodes[i]->clock = 0;
		nodes[i]->status = NODE_STATUS_REQUEST;		
		heap_push(task->heap, nodes[i]);
	}

	locker_lock(&dht->locker);
	list_insert_after(&task->task.link, dht->tasks2.prev);
	locker_unlock(&dht->locker);
	return dht_notify(dht);
}

int dht_get_peers(dht_t* dht, const uint8_t info_hash[20])
{
	int i, r;
	struct node_t* nodes[N_TASK];
	struct dht_find_task_t* task;

	task = calloc(1, sizeof(*task));
	if (!task) return ENOMEM;

	locker_lock(&dht->locker);
	r = router_nearest(dht->router, info_hash, nodes, sizeof(nodes) / sizeof(nodes[0]));
	locker_unlock(&dht->locker);
	if (0 == r)
	{
		free(task);
		return ENOENT; // don't have any node, add bootstrap node first
	}

	memcpy(&task->id, info_hash, sizeof(task->id));
	task->task.transaction = 0; // placeholder
	task->task.type = DHT_GET_PEERS;

	task->heap = heap_create(node_compare_great, task->id);
	heap_reserve(task->heap, sizeof(nodes) / sizeof(nodes[0]) + 1);
	for (i = 0; i < r; i++)
	{
		nodes[i]->retry = 0;
		nodes[i]->clock = 0;
		nodes[i]->status = NODE_STATUS_REQUEST;
		heap_push(task->heap, nodes[i]);
	}

	locker_lock(&dht->locker);
	list_insert_after(&task->task.link, dht->tasks2.prev);
	locker_unlock(&dht->locker);
	return dht_notify(dht);
}

int dht_announce(dht_t* dht, const struct sockaddr_storage* addr, const uint8_t info_hash[20], uint16_t port, const uint8_t* token, uint32_t token_bytes)
{
	int r;
	uint8_t buffer[256];
	r = dht_announce_peer_write(buffer, sizeof(buffer), 0, dht->id, info_hash, port, token, token_bytes);
	return udp_socket_sendto(&dht->udp, buffer, r, addr);
}

static void dht_active(struct dht_t* dht, const uint8_t id[20], const struct sockaddr_storage* addr)
{
	struct node_t* node = NULL;
	router_add(dht->router, id, addr, &node);
	if (node)
	{
		node->active = dht->clock;
		node_release(node);
	}
}

static struct dht_task_t* dht_task_find(struct dht_t* dht, uint16_t transaction)
{
	struct list_head* pos;
	struct dht_task_t* task;

	list_for_each(pos, &dht->tasks)
	{
		task = list_entry(pos, struct dht_task_t, link);
		if (task->transaction == transaction)
		{
			return task;
		}
	}
	return NULL;
}

static int dht_handle_ping(void* param, const uint8_t* transaction, uint32_t bytes, const uint8_t id[20])
{
	int n;
	uint8_t buffer[50];
	struct dht_t* dht;
	struct dht_io_t* io;
	io = (struct dht_io_t*)param;
	dht = io->dht;

	// mark id active
	dht_active(dht, id, &io->addr);

	n = dht_pong_write(buffer, sizeof(buffer), transaction, bytes, dht->id);
	return udp_socket_sendto(&dht->udp, buffer, n, &io->addr);
}

static int dht_handle_pong(void* param, int code, uint16_t transaction, const uint8_t id[20])
{
	struct dht_t* dht;
	struct dht_io_t* io;
	struct dht_ping_task_t* task;
	io = (struct dht_io_t*)param;
	dht = io->dht;

	if (0 == code)
	{
		// mark id active
		dht_active(dht, id, &io->addr);
	}

	task = (struct dht_ping_task_t*)dht_task_find(dht, transaction);
	if (!task)
	{
		assert(0);
		return 0;
	}

	// remove task
	list_remove(&task->task.link);
	free(task);
	return 0;
}

static int dht_handle_find_node(void* param, const uint8_t* transaction, uint32_t bytes, const uint8_t id[20], const uint8_t target[20])
{
	int n;
	uint8_t buffer[256];
	struct node_t* nodes[8];
	struct dht_t* dht;
	struct dht_io_t* io;
	io = (struct dht_io_t*)param;
	dht = io->dht;

	// make id active
	dht_active(dht, id, &io->addr);

	n = router_nearest(dht->router, target, nodes, sizeof(nodes)/sizeof(nodes[0]));
	n = dht_find_node_reply_write(buffer, sizeof(buffer), transaction, bytes, dht->id, nodes, n);
	return udp_socket_sendto(&dht->udp, buffer, n, &io->addr);
}

static int dht_handle_find_reply(struct dht_io_t* io, struct dht_find_task_t* task, int code, const uint8_t id[20], const struct node_t* nodes, uint32_t count, const uint8_t* token, uint32_t bytes)
{
	int i;
	struct dht_t* dht;
	struct node_t* node;

	dht = io->dht;

	if (0 == code)
	{
		// job done
		for (i = 0; i < heap_size(task->heap); i++)
		{
			node = heap_get(task->heap, i);
			if (0 == memcmp(id, node->id, 20))
			{
				if (token && bytes > 0)
					node_settoken(node, token, bytes);
				node->active = dht->clock;
				node->status = NODE_STATUS_REPLY;
				break;
			}
		}
		assert(i < heap_size(task->heap));

		// update router table
		for (i = 0; i < (int)count; i++)
		{
			node = NULL;
			if (0 == router_add(dht->router, nodes[i].id, &nodes[i].addr, &node))
			{
				// update nearest NODE
				struct node_t* top = NULL;
				node_addref(node);
				heap_push(task->heap, node);
				if (heap_size(task->heap) > N_TASK)
				{
					top = (struct node_t*)heap_top(task->heap);
					node->status = NODE_STATUS_UNKNOWN; // clear
					node_release(top);
					heap_pop(task->heap);
				}

				if (top != node)
				{
					// find nearer new node
					node->status = NODE_STATUS_REQUEST;
					node->retry = 0;
					node->active = 0;
				}
			}

			if (node)
				node_release(node);
		}
	}
	else
	{
		// try by addr
		for (i = 0; i < heap_size(task->heap); i++)
		{
			node = heap_get(task->heap, i);
			if (0 == memcmp(&node->addr, &io->addr, io->addrlen))
			{
				node->active = dht->clock;
				node->status = NODE_STATUS_ERROR;
				node->retry = code;
				break;
			}
		}
	}

	return 0;
}

static int dht_handle_find_node_reply(void* param, int code, uint16_t transaction, const uint8_t id[20], const struct node_t* nodes, uint32_t count)
{
	struct dht_t* dht;
	struct dht_io_t* io;
	struct dht_find_task_t* task;
	io = (struct dht_io_t*)param;
	dht = io->dht;

	if (0 == code)
	{
		// mark id active
		dht_active(dht, id, &io->addr);
	}

	task = (struct dht_find_task_t*)dht_task_find(dht, transaction);
	if (!task)
	{
		assert(0);
		return 0;
	}

	dht_handle_find_reply(io, task, code, id, nodes, count, NULL, 0);

	return dht_schedule_find_node(dht, task);
}

static int dht_handle_get_peers(void* param, const uint8_t* transaction, uint32_t bytes, const uint8_t id[20], const uint8_t info_hash[20])
{
	int n, peernum;
	uint8_t token[20];
	uint8_t buffer[256];
	struct node_t* nodes[8];
	struct sockaddr_storage addrs[50];
	struct dht_t* dht;
	struct dht_io_t* io;
	io = (struct dht_io_t*)param;
	dht = io->dht;
	
	// create token
	memcmp(token, id, sizeof(token));

	// make id active
	dht_active(dht, id, &io->addr);
	
	// get torrent peers
	peernum = dht->handler.query_peers(dht->param, info_hash, addrs, sizeof(addrs)/sizeof(addrs[0]));
	
	// get router nearest nodes
	n = router_nearest(dht->router, info_hash, nodes, sizeof(nodes) / sizeof(nodes[0]));
	n = dht_get_peers_reply_write(buffer, sizeof(buffer), transaction, bytes, dht->id, token, sizeof(token), nodes, n, addrs, peernum);
	return udp_socket_sendto(&dht->udp, buffer, n, &io->addr);
}

static int dht_handle_get_peers_reply(void* param, int code, uint16_t transaction, const uint8_t id[20], const uint8_t* token, uint32_t bytes, const struct node_t* nodes, uint32_t count, const struct sockaddr_storage* peers, uint32_t peernum)
{
	struct dht_t* dht;
	struct dht_io_t* io;
	struct dht_find_task_t* task;
	io = (struct dht_io_t*)param;
	dht = io->dht;

	if (0 == code)
	{
		// mark id active
		dht_active(dht, id, &io->addr);
	}

	task = (struct dht_find_task_t*)dht_task_find(dht, transaction);
	if (!task)
	{
		assert(0);
		return 0;
	}

	dht_handle_find_reply(io, task, code, id, nodes, count, token, bytes);

	if (0 == code && peernum > 0)
	{
		// update torrent peers
		task->peers += peernum;
		dht->handler.get_peers(dht->param, code, task->id, peers, peernum);		
	}

	return dht_schedule_get_peers(dht, task);
}

static int dht_handle_announce_peer(void* param, const uint8_t* transaction, uint32_t bytes, const uint8_t id[20], const uint8_t info_hash[20], uint16_t port, const uint8_t* token, uint32_t token_bytes)
{
	int n;
	uint8_t buffer[256];
	struct dht_t* dht;
	struct dht_io_t* io;
	io = (struct dht_io_t*)param;
	dht = io->dht;

	// check token
	assert(20 == token_bytes);
	assert(0 == memcmp(id, token, 20));

	// make id active
	dht_active(dht, id, &io->addr);

	dht->handler.announce_peer(dht->param, info_hash, port, &io->addr);

	// reply
	n = dht_announce_peer_reply_write(buffer, sizeof(buffer), transaction, bytes, dht->id);
	return udp_socket_sendto(&dht->udp, buffer, n, &io->addr);
}

static int dht_handle_announce_peer_reply(void* param, int code, uint16_t transaction, const uint8_t id[20])
{
	struct dht_t* dht;
	struct dht_io_t* io;
	io = (struct dht_io_t*)param;
	dht = io->dht;

	if (0 == code)
	{
		// make id active
		dht_active(dht, id, &io->addr);
	}

	(void)transaction; // unused
	return 0;
}

static int dht_schedule_ping(struct dht_t* dht, struct dht_ping_task_t* task)
{
	int r;
	uint8_t buffer[64];

	if (task->clock + N_TIMEOUT < dht->clock)
	{
		if (task->retry++ < 4)
		{
			r = dht_ping_write(buffer, sizeof(buffer), task->task.transaction, dht->id);
			r = udp_socket_sendto(&dht->udp, buffer, r, &task->addr);
			if (r > 0)
			{
				task->clock = dht->clock;
			}
		}
		else
		{
			list_remove(&task->task.link);
			free(task);

			if(dht->handler.ping)
				dht->handler.ping(dht->param, ETIMEDOUT);
		}
	}

	return 0;
}

static int dht_schedule_find_node(struct dht_t* dht, struct dht_find_task_t* task)
{
	int i, r, n;
	uint8_t buffer[128];
	struct node_t* node;

	for (n = i = 0; i < heap_size(task->heap); i++)
	{
		node = (struct node_t*)heap_get(task->heap, i);
		assert(NODE_STATUS_UNKNOWN != node->status);
		if (NODE_STATUS_REQUEST != node->status)
		{
			n++;
			continue;
		}

		if (node->clock + N_TIMEOUT < dht->clock)
		{
			if (node->retry++ < 4)
			{
				r = dht_find_node_write(buffer, sizeof(buffer), task->task.transaction, dht->id, task->id);
				r = udp_socket_sendto(&dht->udp, buffer, r, &node->addr);
				if (r > 0)
					node->clock = dht->clock;
			}
			else
			{
				node->status = NODE_STATUS_ERROR;
				n++;
			}
		}
	}

	if (n == heap_size(task->heap))
	{
		// all done
		list_remove(&task->task.link);
		if(dht->handler.find_node)
			dht->handler.find_node(dht->param, 0, task->id);
		dht_find_task_free(task);
	}
	else
	{
		// schedule later
	}

	return 0;
}

static int dht_schedule_get_peers(struct dht_t* dht, struct dht_find_task_t* task)
{
	int i, r, n;
	uint8_t buffer[128];
	struct node_t* node;

	for (n = i = 0; i < heap_size(task->heap); i++)
	{
		node = (struct node_t*)heap_get(task->heap, i);
		assert(NODE_STATUS_UNKNOWN != node->status);
		if (NODE_STATUS_REQUEST != node->status)
		{
			n++;
			continue;
		}

		if (node->clock + N_TIMEOUT < dht->clock)
		{
			if (node->retry++ < 4)
			{
				r = dht_get_peers_write(buffer, sizeof(buffer), task->task.transaction, dht->id, task->id);
				r = udp_socket_sendto(&dht->udp, buffer, r, &node->addr);
				if (r > 0)
					node->clock = dht->clock;
			}
			else
			{
				node->status = NODE_STATUS_ERROR;
				n++;
			}
		}
	}

	if (n == heap_size(task->heap))
	{
		if (0 == task->peers)
		{
			// don't find peers
			// TODO: restart from bootstrap ???
		}

		// all done
		list_remove(&task->task.link);
		if (dht->handler.get_peers)
			dht->handler.get_peers(dht->param, 0, task->id, NULL, 0);
		dht_find_task_free(task);
	}
	else
	{
		// schedule later
	}

	return 0;
}

static int dht_schedule(struct dht_t* dht)
{
	int r = 0;
	struct list_head* pos, *next;
	struct dht_task_t* task;
	
	list_for_each_safe(pos, next, &dht->tasks)
	{
		task = list_entry(pos, struct dht_task_t, link);
		switch (task->type)
		{
		case DHT_PING:
			r = dht_schedule_ping(dht, (struct dht_ping_task_t*)task);
			break;

		case DHT_FIND_NODE:
			r = dht_schedule_find_node(dht, (struct dht_find_task_t*)task);
			break;

		case DHT_GET_PEERS:
			r = dht_schedule_get_peers(dht, (struct dht_find_task_t*)task);
			break;

		default:
			assert(0);
			r = -1;
		}
	}

	return r;
}

static uint16_t dht_transaction_id(struct dht_t* dht)
{
	int32_t id, n = 0;
	struct list_head *pos;
	struct dht_task_t* task;

	do
	{
		id = atomic_increment32(&dht->transaction_id);
		list_for_each(pos, &dht->tasks)
		{
			task = list_entry(pos, struct dht_task_t, link);
			if (task->transaction == id)
				break;
		}
		
		n++;
	} while (pos != &dht->tasks || n > 65536);

	assert(n < 65535); // too many tasks
	return (uint16_t)id;
}

static void dht_notify_handler(struct dht_t* dht)
{
	struct list_head* pos, *next;

	locker_lock(&dht->locker);
	list_for_each_safe(pos, next, &dht->tasks2)
	{
		(list_entry(pos, struct dht_task_t, link))->transaction = dht_transaction_id(dht);
		list_remove(pos);
		list_insert_before(pos, dht->tasks.next);
	}
	locker_unlock(&dht->locker);

	dht_schedule(dht);
}

static int dht_select_read(struct dht_t* dht, struct dht_message_handler_t* handler, int timeout)
{
	int i, r;
	fd_set fds;
	socket_t udp[2];
	struct timeval tv;
	struct dht_io_t io;

	FD_ZERO(&fds);
	udp[0] = dht->udp.udp;
	udp[1] = dht->udp.udp6;
	for (i = 0; i < 2; i++)
	{
		if (socket_invalid != udp[i])
			FD_SET(udp[i], &fds);
	}

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;
	r = socket_select_readfds((dht->udp.udp > dht->udp.udp6 ? dht->udp.udp: dht->udp.udp6) + 1, &fds, timeout < 0 ? NULL : &tv);
	dht->clock = system_clock();

	if (r <= 0) return r;

	io.dht = dht;
	for (i = 0; i < 2; i++)
	{
		if (!FD_ISSET(udp[i], &fds))
			continue;

		io.addrlen = sizeof(io.addr);
		memset(&io.addr, 0, sizeof(io.addr));
		r = socket_recvfrom(udp[i], dht->buffer, sizeof(dht->buffer), 0, (struct sockaddr*)&io.addr, &io.addrlen);
		if (r <= 0)
		{
			app_log(LOG_ERROR, "%s socket_recvfrom ret: %d\n", 0==i ? "IPv4" : "IPv6", r);
		}
		else if (N_NOTIFY == r)
		{
			assert(0xAB == dht->buffer[0] && 0xCD == dht->buffer[1] && 0xEF == dht->buffer[2]);
			dht_notify_handler(dht);
		}
		else
		{
			dht_message_read(dht->buffer, r, handler, &io);
		}
	}

	return 1;
}

static int STDCALL dht_process(void* param)
{
	int r = 0;
	struct dht_t* dht;
	struct dht_message_handler_t handler;
	dht = (struct dht_t*)param;
	
	handler.ping = dht_handle_ping;
	handler.pong = dht_handle_pong;
	handler.find_node = dht_handle_find_node;
	handler.find_node_reply = dht_handle_find_node_reply;
	handler.get_peers = dht_handle_get_peers;
	handler.get_peers_reply = dht_handle_get_peers_reply;
	handler.announce_peer = dht_handle_announce_peer;
	handler.announce_peer_reply = dht_handle_announce_peer_reply;

	while (dht->running)
	{
		r = dht_select_read(dht, &handler, N_TIMEOUT);
		if (1 == r)
		{
			// don't need do anything
		}
		else if (0 == r)
		{
			// timeout
			dht_schedule(dht);
		}
		else
		{
			// error
			assert(r < 0);
			break;
		}

		// notify miss ???
		if (!list_empty(&dht->tasks2))
		{
			dht_notify_handler(dht);
		}

		// TODO: keep alive task
	}

	app_log(LOG_INFO, "%s thread exit: %d\n", __FUNCTION__, r);
	return 0;
}
