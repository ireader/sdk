#include "stun-agent.h"
#include "stun-proto.h"
#include "sockutil.h"
#include "port/ip-route.h"
#include <errno.h>

#define TURN_PEER_PORT 2345
#define TURN_PEER_IP "128.10.2.2"
#define TURN_SERVER "stun.linphone.org"
#define TURN_USR "tao3"
#define TURN_PWD "123456"
//#define TURN_SERVER "numb.viagenie.ca"
//#define TURN_USR "tao3@outlook.com"
//#define TURN_PWD "12345678"

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

static void turn_ondata(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int byte)
{
}

extern "C" void turn_client_test()
{
	int r, protocol;
	char local[40];
	sockaddr_storage host, server, peer, reflexive, relayed;
	socklen_t addrlen = sizeof(struct sockaddr_in);

	socket_init();
	r = socket_addr_from_ipv4((struct sockaddr_in*)&server, TURN_SERVER, STUN_PORT); assert(0 == r);
    r = socket_addr_from_ipv4((struct sockaddr_in*)&peer, TURN_PEER_IP, TURN_PEER_PORT); assert(0 == r);
	r = ip_route_get(TURN_SERVER, local); assert(0 == r);

	struct turn_client_test_context_t ctx;

	uint8_t data[1400];
	struct stun_agent_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = turn_send;
	handler.ondata = turn_ondata;

	memset(&ctx, 0, sizeof(ctx));
	ctx.udp = socket_udp_bind_ipv4(local, 0);
	ctx.stun = stun_agent_create(STUN_RFC_5389, &handler, &ctx);

	getsockname(ctx.udp, (struct sockaddr*)&host, &addrlen);

	stun_request_t* req = stun_request_create(ctx.stun, STUN_RFC_5389, turn_onallocate, &ctx);
	stun_request_setaddr(req, STUN_PROTOCOL_UDP, (struct sockaddr*)&host, (const struct sockaddr*)&server, NULL);
    stun_request_setauth(req, STUN_CREDENTIAL_LONG_TERM, TURN_USR, TURN_PWD, "", "");
    r = turn_agent_allocate(req, TURN_TRANSPORT_UDP); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen); assert(r > 0);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r); assert(0 == r);
    r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen); assert(r > 0);
    r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r); assert(0 == r);
	stun_request_getaddr(req, &protocol, &host, &peer, &reflexive, &relayed); assert(0 == r);

	stun_request_t* req2 = stun_request_create(ctx.stun, STUN_RFC_5389, turn_onrefresh, &ctx);
	stun_request_setaddr(req2, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, NULL);
    stun_request_setauth(req2, STUN_CREDENTIAL_LONG_TERM, ctx.usr, ctx.pwd, ctx.realm, ctx.nonce);
	r = turn_agent_refresh(req2, 600); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen); assert(r > 0);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r); assert(0 == r);

	stun_request_t* req3 = stun_request_create(ctx.stun, STUN_RFC_5389, turn_oncreate_permission, &ctx);
	stun_request_setaddr(req3, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, NULL);
    stun_request_setauth(req3, STUN_CREDENTIAL_LONG_TERM, ctx.usr, ctx.pwd, ctx.realm, ctx.nonce);
    r = turn_agent_create_permission(req3, (const struct sockaddr*)&peer); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen); assert(r > 0);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r); assert(0 == r);

	stun_request_t* req4 = stun_request_create(ctx.stun, STUN_RFC_5389, NULL, &ctx);
	stun_request_setaddr(req4, STUN_PROTOCOL_UDP, (sockaddr*)&host, (const struct sockaddr*)&server, NULL);
    stun_request_setauth(req4, STUN_CREDENTIAL_LONG_TERM, ctx.usr, ctx.pwd, ctx.realm, ctx.nonce);
    r = turn_agent_send(ctx.stun, (sockaddr*)&relayed, (const struct sockaddr*)&peer, "hello TURN", 10); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen);
    if(r < 0 && 4 == errno) r = r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen); assert(r > 0);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r); assert(0 == r);

	stun_request_t* req5 = stun_request_create(ctx.stun, STUN_RFC_5389, turn_onchannel_bind, &ctx);
	stun_request_setaddr(req5, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, NULL);
    stun_request_setauth(req5, STUN_CREDENTIAL_LONG_TERM, ctx.usr, ctx.pwd, ctx.realm, ctx.nonce);
    r = turn_agent_channel_bind(req5, (const struct sockaddr*)&peer, 0x4000); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r); assert(0 == r);

	stun_request_t* req6 = stun_request_create(ctx.stun, STUN_RFC_5389, NULL, &ctx);
	stun_request_setaddr(req6, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, NULL);
    stun_request_setauth(req6, STUN_CREDENTIAL_LONG_TERM, ctx.usr, ctx.pwd, ctx.realm, ctx.nonce);
    r = turn_agent_send(ctx.stun, (const struct sockaddr*)&relayed, (const struct sockaddr*)&peer, "hello TURN channel", 18); assert(0 == r);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr*)&host, (const struct sockaddr*)&server, data, r); assert(0 == r);

	stun_agent_destroy(&ctx.stun);
	socket_close(ctx.udp);
	socket_cleanup();
}
