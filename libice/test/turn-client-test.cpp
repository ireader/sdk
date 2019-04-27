#include "stun-agent.h"
#include "stun-proto.h"
#include "sockutil.h"
#include <errno.h>

struct turn_client_test_context_t
{
	socket_t udp;
	stun_agent_t* stun;
    char usr[512];
    char pwd[512];
    char realm[128];
    char nonce[128];
};

static int turn_send(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	struct turn_client_test_context_t* ctx = (struct turn_client_test_context_t*)param;

	if (STUN_PROTOCOL_UDP == protocol)
	{
        assert(AF_INET == remote->sa_family || AF_INET6 == remote->sa_family);
        if(AF_INET == local->sa_family || AF_INET6 == local->sa_family)
            socket_bind(ctx->udp, local, socket_addr_len(local));
        int r = socket_sendto(ctx->udp, data, bytes, 0, remote, socket_addr_len(remote));
        assert(r == bytes);
	}
	else
	{
		assert(0);
	}
	return 0;
}

static int turn_onallocate(void* param, const stun_request_t* req, int code, const char* phrase)
{
    struct turn_client_test_context_t* ctx = (struct turn_client_test_context_t*)param;
    if(0 == code)
        stun_request_getauth(req, ctx->usr, ctx->pwd, ctx->realm, ctx->nonce);
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
    //r = socket_addr_from_ipv4(&server, "numb.viagenie.ca", STUN_PORT); assert(0 == r);
    r = socket_addr_from_ipv4(&server, "127.0.0.1", STUN_PORT); assert(0 == r);
    r = socket_addr_from_ipv4(&peer, "127.0.0.1", 2345); assert(0 == r);

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
    stun_request_setauth(req, STUN_CREDENTIAL_LONG_TERM, "demo", "demo", "", "");
    //stun_request_setauth(req, STUN_CREDENTIAL_LONG_TERM, "tao3@outlook.com", "12345678", "", "");
	r = turn_agent_allocate(req, turn_ondata, &ctx); assert(0 == r);
	getsockname(ctx.udp, (struct sockaddr*)&host, &addrlen);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen); assert(r > 0);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r); assert(0 == r);
    r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen); assert(r > 0);
    r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r); assert(0 == r);

	stun_request_t* req2 = stun_request_create(ctx.stun, STUN_RFC_5389, turn_onrefresh, &ctx);
	stun_request_setaddr(req2, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server);
    stun_request_setauth(req2, STUN_CREDENTIAL_LONG_TERM, ctx.usr, ctx.pwd, ctx.realm, ctx.nonce);
	r = turn_agent_refresh(req2, 600); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen); assert(r > 0);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r); assert(0 == r);

	stun_request_t* req3 = stun_request_create(ctx.stun, STUN_RFC_5389, turn_oncreate_permission, &ctx);
	stun_request_setaddr(req3, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server);
    stun_request_setauth(req3, STUN_CREDENTIAL_LONG_TERM, ctx.usr, ctx.pwd, ctx.realm, ctx.nonce);
    r = turn_agent_create_permission(req3, (const struct sockaddr*)&peer); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen); assert(r > 0);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r); assert(0 == r);

	stun_request_t* req4 = stun_request_create(ctx.stun, STUN_RFC_5389, NULL, &ctx);
	stun_request_setaddr(req4, STUN_PROTOCOL_UDP, (sockaddr*)&host, (const struct sockaddr*)&server);
    stun_request_setauth(req4, STUN_CREDENTIAL_LONG_TERM, ctx.usr, ctx.pwd, ctx.realm, ctx.nonce);
    r = turn_agent_send(req4, (const struct sockaddr*)&peer, "hello TURN", 10); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen);
    if(r < 0 && 4 == errno) r = r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen); assert(r > 0);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r); assert(0 == r);

	stun_request_t* req5 = stun_request_create(ctx.stun, STUN_RFC_5389, turn_onchannel_bind, &ctx);
	stun_request_setaddr(req5, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server);
    stun_request_setauth(req5, STUN_CREDENTIAL_LONG_TERM, ctx.usr, ctx.pwd, ctx.realm, ctx.nonce);
    r = turn_agent_channel_bind(req5, (const struct sockaddr*)&peer, 0x4000); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r); assert(0 == r);

	stun_request_t* req6 = stun_request_create(ctx.stun, STUN_RFC_5389, NULL, &ctx);
	stun_request_setaddr(req6, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server);
    stun_request_setauth(req6, STUN_CREDENTIAL_LONG_TERM, ctx.usr, ctx.pwd, ctx.realm, ctx.nonce);
    r = turn_agent_send(req6, (const struct sockaddr*)&peer, "hello TURN channel", 18); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r); assert(0 == r);

	stun_agent_destroy(&ctx.stun);
	socket_close(ctx.udp);
	socket_cleanup();
}
