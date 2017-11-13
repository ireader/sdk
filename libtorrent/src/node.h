#ifndef _node_h_
#define _node_h_

#include "sys/sock.h"
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define N_NODEID 20

enum
{
	NODE_STATUS_UNKNOWN = 0,
	NODE_STATUS_REQUEST,
	NODE_STATUS_REPLY,
	NODE_STATUS_ERROR,
};

struct node_t
{
	int32_t ref;
	uint8_t id[N_NODEID];
	struct sockaddr_storage addr;

	int retry;
	int status; // STATUS
	uint64_t clock; // last request clock
	uint64_t active; // last request/reply clock

	uint8_t* token;
	unsigned int bytes;
	unsigned int capacity;
};

struct node_t* node_create();
struct node_t* node_create2(const uint8_t id[N_NODEID], const struct sockaddr_storage* addr);

int32_t node_addref(struct node_t* node);
int32_t node_release(struct node_t* node);

int node_settoken(struct node_t* node, const uint8_t* token, unsigned int bytes);
int node_compare_less(const uint8_t ref[N_NODEID], const struct node_t* l, const struct node_t* r);
int node_compare_great(const uint8_t ref[N_NODEID], const struct node_t* l, const struct node_t* r);

#if defined(__cplusplus)
}
#endif
#endif /* !_node_h_ */
