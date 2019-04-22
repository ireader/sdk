#include "stun-agent.h"
#include "sockutil.h"

struct turn_client_test_context_t
{
	socket_t udp;
	stun_agent_t* stun;
};

static int turn_send(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	struct turn_client_test_context_t* ctx = (struct turn_client_test_context_t*)param;

	if (STUN_PROTOCOL_UDP == protocol)
	{
		assert(AF_INET == local->sa_family && AF_INET == remote->sa_family);
		socket_bind(ctx->udp, local, socket_addr_len(local));
		socket_sendto(ctx->udp, data, bytes, 0, remote, socket_addr_len(remote));
	}
	else
	{
		assert(0);
	}
	return 0;
}

static int turn_onallocate(void* param, const stun_request_t* req, int code, const char* phrase)
{
	return 0;
}

static int turn_onrefresh(void* param, const stun_request_t* req, int code, const char* phrase)
{
	return 0;
}

static int turn_oncreate_permission(void* param, const stun_request_t* req, int code, const char* phrase)
{
	return 0;
}

static int turn_onchannel_bind(void* param, const stun_request_t* req, int code, const char* phrase)
{
	return 0;
}

static void turn_ondata(void* param, const void* data, int byte, int protocol, const struct sockaddr* local, const struct sockaddr* remote)
{
}

extern "C" void turn_client_test()
{
	int r;
	sockaddr_in host, server, peer;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	//r = socket_addr_from_ipv4(&addr, &addrlen, "stunserver.org", 3478);
	r = socket_addr_from_ipv4(&server, "stun.stunprotocol.org", 3478); assert(0 == r);
	r = socket_addr_from_ipv4(&peer, "10.14.29.219", 1234); assert(0 == r);

	struct turn_client_test_context_t ctx;

	uint8_t data[1400];
	struct stun_agent_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = turn_send;

	memset(&ctx, 0, sizeof(ctx));
	socket_init();
	ctx.udp = socket_udp();
	ctx.stun = stun_agent_create(STUN_RFC_5389, &handler, &ctx);

	stun_request_t* req = stun_request_create(ctx.stun, STUN_RFC_5389, turn_onallocate, &ctx);
	stun_request_setaddr(req, STUN_PROTOCOL_UDP, NULL, (const struct sockaddr*)&server);
	r = turn_agent_allocate(req, turn_ondata, &ctx); assert(0 == r);
	getsockname(ctx.udp, (struct sockaddr*)&host, &addrlen);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r);

	stun_request_t* req2 = stun_request_create(ctx.stun, STUN_RFC_3489, turn_onrefresh, &ctx);
	stun_request_setaddr(req2, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server);
	r = turn_agent_refresh(req2, 600); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r);

	stun_request_t* req3 = stun_request_create(ctx.stun, STUN_RFC_3489, turn_oncreate_permission, &ctx);
	stun_request_setaddr(req3, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server);
	r = turn_agent_create_permission(req3, (const struct sockaddr*)&peer); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r);

	stun_request_t* req4 = stun_request_create(ctx.stun, STUN_RFC_3489, turn_oncreate_permission, &ctx);
	stun_request_setaddr(req4, STUN_PROTOCOL_UDP, (sockaddr*)&host, (const struct sockaddr*)&server);
	r = turn_agent_send(req4, (const struct sockaddr*)&peer, "hello TURN", 10); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r);

	stun_request_t* req5 = stun_request_create(ctx.stun, STUN_RFC_3489, turn_oncreate_permission, &ctx);
	stun_request_setaddr(req5, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server);
	r = turn_agent_channel_bind(req5, (const struct sockaddr*)&peer, 0x4000); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r);

	stun_request_t* req6 = stun_request_create(ctx.stun, STUN_RFC_3489, turn_oncreate_permission, &ctx);
	stun_request_setaddr(req6, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server);
	r = turn_agent_send(req6, (const struct sockaddr*)&peer, "hello TURN channel", 18); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r);

	stun_agent_destroy(&ctx.stun);
	socket_close(ctx.udp);
	socket_cleanup();
}
