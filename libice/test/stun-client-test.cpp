#include "stun-agent.h"
#include "stun-proto.h"
#include "tls-socket.h"
#include "sockutil.h"
#include <stdlib.h>

// https://gist.github.com/zziuni/3741933
// https://gist.github.com/mondain/b0ec1cf5f60ae726202e
// http://olegh.ftp.sh/public-stun.txt
// https://stackoverflow.com/questions/20068944/webrtc-stun-stun-l-google-com19302

struct stun_client_test_context_t
{
	socket_t udp;
	tls_socket_t* ssl;
	stun_agent_t* stun;
	char usr[512];
	char pwd[512];
    char realm[128];
    char nonce[128];
};

static int stun_send(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	struct stun_client_test_context_t* ctx = (struct stun_client_test_context_t*)param;

	if (STUN_PROTOCOL_TLS == protocol)
	{
		int n = tls_socket_write(ctx->ssl, data, bytes);
		assert(bytes == n);
	}
	else if (STUN_PROTOCOL_UDP == protocol)
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

static int stun_onshared_secret(void* param, const stun_request_t* req, int code, const char* phrase)
{
	struct stun_client_test_context_t* ctx = (struct stun_client_test_context_t*)param;
	if (0 == code)
    {
        stun_request_getauth(req, ctx->usr, ctx->pwd, ctx->realm, ctx->nonce);
	}
	return 0;
}

static int stun_onbind(void* param, const stun_request_t* req, int code, const char* phrase)
{
	int protocol;
	struct sockaddr_storage reflexive;
	struct stun_client_test_context_t* ctx = (struct stun_client_test_context_t*)param;
	if (0 == code)
	{
		stun_request_getaddr(req, &protocol, NULL, NULL, &reflexive, NULL);
	}
	return 0;
}

extern "C" void stun_client_test()
{
	socket_init();
	tls_socket_init();

	int r;
    sockaddr_in host, server;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	//r = socket_addr_from_ipv4(&server, "stunserver.org", 3478); assert(0 == r);
	//r = socket_addr_from_ipv4(&server, "stun.stunprotocol.org", STUN_PORT); assert(0 == r);
	r = socket_addr_from_ipv4(&server, "numb.viagenie.ca", STUN_PORT); assert(0 == r);
    
	struct stun_client_test_context_t ctx;
	memset(&ctx, 0, sizeof(ctx));

	uint8_t data[1400];
	struct stun_agent_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = stun_send;

	ctx.udp = socket_udp();
//	ctx.ssl = tls_socket_connect2((const struct sockaddr*)&server, 5000);
	ctx.stun = stun_agent_create(STUN_RFC_3489, &handler, &ctx);

//    stun_request_t* req = stun_request_create(ctx.stun, STUN_RFC_3489, stun_onshared_secret, &ctx);
//    stun_request_setaddr(req, STUN_PROTOCOL_TLS, NULL, (const struct sockaddr*)&server);
//    r = stun_agent_shared_secret(req); assert(0 == r);
//    r = tls_socket_read(ctx.ssl, data, sizeof(data)); assert(r > 0);
//    r = stun_agent_input(ctx.stun, STUN_PROTOCOL_TLS, NULL, (const struct sockaddr*)&server, data, r);

	r = socket_addr_from_ipv4(&server, "stun.stunprotocol.org", STUN_PORT); assert(0 == r);
    stun_request_t* req2 = stun_request_create(ctx.stun, STUN_RFC_3489, stun_onbind, &ctx);
	stun_request_setaddr(req2, STUN_PROTOCOL_UDP, NULL, (const struct sockaddr*)&server, NULL);
	r = stun_agent_bind(req2); assert(0 == r);
    addrlen = sizeof(host);
	r = socket_recvfrom(ctx.udp, data, sizeof(data), 0, (struct sockaddr*)&server, &addrlen);
	r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, NULL, (const struct sockaddr*)&server, data, r);

	stun_agent_destroy(&ctx.stun);
    if(ctx.ssl) tls_socket_close(ctx.ssl);
	socket_close(ctx.udp);
    tls_socket_cleanup();
	socket_cleanup();
}
