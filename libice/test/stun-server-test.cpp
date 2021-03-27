#include "stun-agent.h"
#include "stun-proto.h"
#include "tls-socket.h"
#include "sockutil.h"
#include "sys/pollfd.h"
#include "aio-timeout.h"
#include <stdlib.h>
#include <time.h>
#include <string>
#include <map>

typedef std::map<socket_t, struct sockaddr_storage> TSockets;
typedef std::map<tls_socket_t*, struct sockaddr_storage> TLSSockets;
struct stun_server_test_context_t
{
    TSockets udprelays;
    TSockets tcpclients;
    TLSSockets tlsclients;
 
    socket_t udp;
    socket_t tcp;
    socket_t tls;
    stun_agent_t* stun;
    char usr[512];
    char pwd[512];
    char realm[128];
    char nonce[128];
};

static int rtp_socket_create(struct sockaddr_in* addr, socket_t rtp[2], unsigned short port[2]);

inline std::string socket_addr_to(const struct sockaddr* addr)
{
    char ip[SOCKET_ADDRLEN + 10];
    u_short port;
    socket_addr_to(addr, socket_addr_len(addr), ip, &port);
    sprintf(ip + strlen(ip), ":%hu", port);
    return std::string(ip);
}

inline std::string socket_addr_to(const struct sockaddr_storage* addr)
{
    return socket_addr_to((const struct sockaddr*)addr);
}

static int stun_send(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
    struct stun_server_test_context_t* ctx = (struct stun_server_test_context_t*)param;
    
    if (STUN_PROTOCOL_TLS == protocol)
    {
        TLSSockets::iterator it;
        for(it = ctx->tlsclients.begin(); it != ctx->tlsclients.end(); it++)
        {
            if(0 == socket_addr_compare((const struct sockaddr*)&it->second, remote))
            {
                struct sockaddr_storage local2;
                socklen_t addrlen = sizeof(struct sockaddr_storage);
                getsockname(tls_socket_getfd(it->first), (struct sockaddr*)&local2, &addrlen);
                if(NULL == local || 0 == socket_addr_compare((const struct sockaddr*)&local2, local))
                    break;
            }
        }
        
        assert(it != ctx->tlsclients.end());
        int n = tls_socket_write(it->first, data, bytes);
        assert(bytes == n);
    }
    else if (STUN_PROTOCOL_UDP == protocol)
    {
        // relay
        TSockets::iterator it;
        for(it = ctx->udprelays.begin(); it != ctx->udprelays.end(); it++)
        {
            if(0 == socket_addr_compare((const struct sockaddr*)&it->second, local))
            {
                printf("udp relay addr: %s, local: %s ==> remote: %s, bytes: %d\n", socket_addr_to(&it->second).c_str(), socket_addr_to(local).c_str(), socket_addr_to(remote).c_str(), bytes);
                int r = socket_sendto(it->first, data, bytes, 0, remote, socket_addr_len(remote));
                assert(r == bytes);
                return 0;
            }
        }
        
        // stun/turn protocol
        assert(AF_INET == remote->sa_family || AF_INET6 == remote->sa_family);
		assert(AF_INET == local->sa_family || AF_INET6 == local->sa_family);
        int r = socket_sendto(ctx->udp, data, bytes, 0, remote, socket_addr_len(remote));
        assert(r == bytes);
    }
    else if (STUN_PROTOCOL_TCP == protocol)
    {
        TSockets::iterator it;
        for(it = ctx->tcpclients.begin(); it != ctx->tcpclients.end(); it++)
        {
            if(0 == socket_addr_compare((const struct sockaddr*)&it->second, remote))
            {
                struct sockaddr_storage local2;
                socklen_t addrlen = sizeof(struct sockaddr_storage);
                getsockname(it->first, (struct sockaddr*)&local2, &addrlen);
                if(NULL == local || 0 == socket_addr_compare((const struct sockaddr*)&local2, local))
                    break;
            }
        }
        
        assert(it != ctx->tcpclients.end());
        int n = socket_send(it->first, data, bytes, 0);
        assert(bytes == n);
    }
	else if (STUN_PROTOCOL_DTLS == protocol)
	{
		assert(0);
	}
    else
    {
        assert(0);
    }
    return 0;
}

static int stun_auth(void* param, int cred, const char* usr, const char* realm, const char* nonce, char pwd[512])
{
    struct stun_server_test_context_t* ctx = (struct stun_server_test_context_t*)param;
    assert(STUN_CREDENTIAL_SHORT_TERM == cred || STUN_CREDENTIAL_LONG_TERM == cred);
    
    if(0 != strcmp(usr, "demo"))
        return -1; // invalid user
    
    if(STUN_CREDENTIAL_LONG_TERM == cred)
    {
        if(0 != strcmp(realm, "demo.com") || 0 != strcmp(nonce, "secret nonce string"))
            return -1;
    }
    
    int n = snprintf(pwd, 512, "%s", "demo");
    return n > 0 && n < 512 ? 0 : -1;
}

/// turn long-term credential get realm/nonce
static int stun_getnonce(void* param, char realm[128], char nonce[128])
{
    struct stun_server_test_context_t* ctx = (struct stun_server_test_context_t*)param;
    snprintf(realm, 128, "%s", "demo.com");
    snprintf(nonce, 128, "%s", "secret nonce string");
    return 0;
}

static int stun_onshared_secret(void* param, stun_response_t* resp, const stun_request_t* req)
{
    int protocol;
    struct sockaddr_storage local, remote;
    struct stun_server_test_context_t* ctx = (struct stun_server_test_context_t*)param;
    stun_request_getaddr(req, &protocol, &local, &remote, NULL, NULL);
    printf("stun_onshared_secret: client(%s) -> server(%s), protocol: %d\n", socket_addr_to(&remote).c_str(), socket_addr_to(&local).c_str(), protocol);
    return stun_agent_shared_secret_response(resp, 200, "OK", "demo", "demo");
}

static int stun_onbind(void* param, stun_response_t* resp, const stun_request_t* req)
{
    int protocol;
    struct sockaddr_storage local, remote;
    struct stun_server_test_context_t* ctx = (struct stun_server_test_context_t*)param;
    stun_request_getaddr(req, &protocol, &local, &remote, NULL, NULL);
    printf("stun_onbind: client(%s) -> server(%s), protocol: %d\n", socket_addr_to(&remote).c_str(), socket_addr_to(&local).c_str(), protocol);
    return stun_agent_bind_response(resp, 200, "OK");
}

static int stun_onbindindication(void* param, const stun_request_t* req)
{
    int protocol;
    struct sockaddr_storage local, remote;
    struct stun_server_test_context_t* ctx = (struct stun_server_test_context_t*)param;
    stun_request_getaddr(req, &protocol, &local, &remote, NULL, NULL);
    printf("stun_onbindindication: client(%s) -> server(%s), protocol: %d\n", socket_addr_to(&remote).c_str(), socket_addr_to(&local).c_str(), protocol);
    return 0;
}

static int stun_onallocate(void* param, stun_response_t* resp, const stun_request_t* req, int evenport, int nextport)
{
    int protocol;
    struct sockaddr_storage local, remote, relay;
    struct stun_server_test_context_t* ctx = (struct stun_server_test_context_t*)param;
    stun_request_getaddr(req, &protocol, &local, &remote, NULL, NULL);
    printf("stun_onallocate: client(%s) -> server(%s), protocol: %d\n", socket_addr_to(&remote).c_str(), socket_addr_to(&local).c_str(), protocol);
    
    socket_t udp[2];
    u_short port[2];
    memcpy(&relay, &local, sizeof(relay));
    int r = rtp_socket_create((struct sockaddr_in*)&local, udp, port);
    if(0 != r)
    {
        printf("stun_onallocate: client(%s) -> server(%s), protocol: %d, bind failed: %d\n", socket_addr_to(&remote).c_str(), socket_addr_to(&local).c_str(), protocol, r);
        return turn_agent_allocate_response(resp, NULL, 508, "Insufficient Capacity");
    }
    
    socklen_t addrlen = sizeof(struct sockaddr_storage);
    getsockname(udp[1], (struct sockaddr*)&local, &addrlen);
    std::pair<TSockets::iterator, bool> pr1 = ctx->udprelays.insert(std::make_pair(udp[1], local));
    getsockname(udp[0], (struct sockaddr*)&local, &addrlen);
    std::pair<TSockets::iterator, bool> pr0 = ctx->udprelays.insert(std::make_pair(udp[0], local));
    printf("stun_onallocate: pair: %s / %s, protocol: %d, r=%d\n", socket_addr_to(&pr0.first->second).c_str(), socket_addr_to(&pr1.first->second).c_str(), protocol, r);
    r = turn_agent_allocate_response(resp, (const struct sockaddr*)&local, 200, "OK");
    if(0 != r)
    {
        ctx->udprelays.erase(pr0.second);
        ctx->udprelays.erase(pr1.second);
        socket_close(udp[0]);
    }
    else
    {
        printf("stun_onallocate: client(%s) -> server(%s), protocol: %d, bind relay addr: %s\n", socket_addr_to(&remote).c_str(), socket_addr_to(&local).c_str(), protocol, socket_addr_to(&relay).c_str());
    }
    return r;
}

static int stun_onrefresh(void* param, stun_response_t* resp, const stun_request_t* req, int lifetime)
{
    int protocol;
    struct sockaddr_storage local, remote;
    struct stun_server_test_context_t* ctx = (struct stun_server_test_context_t*)param;
    stun_request_getaddr(req, &protocol, &local, &remote, NULL, NULL);
    printf("stun_onrefresh: client(%s) -> server(%s), protocol: %d, lifetime: %d\n", socket_addr_to(&remote).c_str(), socket_addr_to(&local).c_str(), protocol, lifetime);
    return turn_agent_refresh_response(resp, 200, "OK");
}

static int stun_onpermission(void* param, stun_response_t* resp, const stun_request_t* req, const struct sockaddr* peer)
{
    int protocol;
    struct sockaddr_storage local, remote;
    struct stun_server_test_context_t* ctx = (struct stun_server_test_context_t*)param;
    stun_request_getaddr(req, &protocol, &local, &remote, NULL, NULL);
    printf("stun_onpermission: client(%s) -> server(%s), protocol: %d, peer: %s\n", socket_addr_to(&remote).c_str(), socket_addr_to(&local).c_str(), protocol, socket_addr_to(peer).c_str());
    return turn_agent_refresh_response(resp, 200, "OK");
}

static int stun_onchannel(void* param, stun_response_t* resp, const stun_request_t* req, const struct sockaddr* peer, uint16_t channel)
{
    int protocol;
    struct sockaddr_storage local, remote;
    struct stun_server_test_context_t* ctx = (struct stun_server_test_context_t*)param;
    stun_request_getaddr(req, &protocol, &local, &remote, NULL, NULL);
    printf("stun_onchannel: client(%s) -> server(%s), protocol: %d, peer: %s, channel: 0x%x\n", socket_addr_to(&remote).c_str(), socket_addr_to(&local).c_str(), protocol, socket_addr_to(peer).c_str(), channel);
    return turn_agent_channel_bind_response(resp, 200, "OK");
}

extern "C" void stun_server_test()
{
	int timeout = 5000;
	uint8_t data[2000];
	socket_bufvec_t vec[1];
	socket_setbufvec(vec, 0, data, sizeof(data));
    
    socket_init();
    tls_socket_init();
    
    int r;
    sockaddr_in host, server;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    //r = socket_addr_from_ipv4(&server, "stunserver.org", 3478); assert(0 == r);
    //r = socket_addr_from_ipv4(&server, "stun.stunprotocol.org", STUN_PORT); assert(0 == r);
    //r = socket_addr_from_ipv4(&server, "numb.viagenie.ca", STUN_PORT); assert(0 == r);
    
    struct stun_server_test_context_t ctx;
    
    struct stun_agent_handler_t handler;
    memset(&handler, 0, sizeof(handler));
    handler.send = stun_send;
    handler.auth = stun_auth;
    handler.getnonce = stun_getnonce;
    handler.onbind = stun_onbind;
    handler.onsharedsecret = stun_onshared_secret;
    handler.onbindindication = stun_onbindindication;
    handler.onallocate = stun_onallocate;
    handler.onrefresh = stun_onrefresh;
    handler.onpermission = stun_onpermission;
    handler.onchannel = stun_onchannel;
    
    ctx.udp = socket_udp_bind_ipv4(NULL, STUN_PORT);
	socket_setpktinfo(ctx.udp, 1);
    ctx.tcp = socket_tcp();
    ctx.tls = socket_tcp();
    socket_bind_any_ipv4(ctx.tcp, STUN_PORT);
    socket_bind_any_ipv4(ctx.tls, STUN_TLS_PORT);
    ctx.stun = stun_agent_create(STUN_RFC_5389, &handler, &ctx);

    while(1)
    {
        struct pollfd fds[64];
        
        fds[0].fd = ctx.udp;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
    
        fds[1].fd = ctx.tcp;
        fds[1].events = POLLIN;
        fds[1].revents = 0;
        
        fds[2].fd = ctx.tls;
        fds[2].events = POLLIN;
        fds[2].revents = 0;
        
        int n = 3;
        for(TSockets::const_iterator it = ctx.udprelays.begin(); it != ctx.udprelays.end(); ++it)
        {
            fds[n].fd = it->first;
            fds[n].events = POLLIN;
            fds[n].revents = 0;
            n++;
        }
    
        for(TSockets::const_iterator it = ctx.tcpclients.begin(); it != ctx.tcpclients.end(); ++it)
        {
            fds[n].fd = it->first;
            fds[n].events = POLLIN;
            fds[n].revents = 0;
            n++;
        }
        
        for(TLSSockets::const_iterator it = ctx.tlsclients.begin(); it != ctx.tlsclients.end(); ++it)
        {
            fds[n].fd = tls_socket_getfd(it->first);
            fds[n].events = POLLIN;
            fds[n].revents = 0;
            n++;
        }
    
        r = poll(fds, n, timeout);
        while(-1 == r && EINTR == errno)
            r = poll(fds, n, timeout);
        
        if(r < 0)
        {
            printf("poll error: %d, errno: %d\n", r, errno);
        }
        else if(0 == r)
        {
            continue; // timeout
        }
        
        for(int i = 0; i < n; i++)
        {
            if(0 == (fds[i].revents & POLLIN))
                continue;
            
            if(0 == i)
            {
                // stun/turn protocol
                struct sockaddr_storage local, from;
                socklen_t fromlen = sizeof(struct sockaddr_storage);
                r = socket_recvfrom(fds[i].fd, data, sizeof(data), 0, (struct sockaddr*)&from, &fromlen);
                if(r > 0)
                {
                    addrlen = sizeof(struct sockaddr_storage);
                    getsockname(fds[i].fd, (struct sockaddr*)&local, &addrlen);
                    r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr *)&local, (const struct sockaddr *)&from, data, r);
                    assert(0 == r);
                }
            }
            else if(1 == i)
            {
                // tcp client
                socket_t tcp;
                struct sockaddr_storage from;
                socklen_t fromlen = sizeof(struct sockaddr_storage);
                tcp = socket_accept(ctx.tcp, &from, &fromlen);
                if(tcp != socket_invalid)
                {
                    std::pair<TSockets::iterator, bool> pr = ctx.tcpclients.insert(std::make_pair(tcp, from));
                    assert(pr.second);
                }
            }
            else if(2 == i)
            {
                // tls client
                socket_t tcp;
                struct sockaddr_storage from;
                socklen_t fromlen = sizeof(struct sockaddr_storage);
                tcp = socket_accept(ctx.tls, &from, &fromlen);
                if(tcp != socket_invalid)
                {
                    tls_socket_t* tls = tls_socket_accept(tcp);
                    std::pair<TLSSockets::iterator, bool> pr = ctx.tlsclients.insert(std::make_pair(tls, from));
                    assert(pr.second);
                }
            }
            else if(i > 2 && i < 3 + ctx.udprelays.size())
            {
                // relay
                struct sockaddr_storage local, from;
                socklen_t fromlen = sizeof(struct sockaddr_storage);
                r = socket_recvfrom(fds[i].fd, data, sizeof(data), 0, (struct sockaddr*)&from, &fromlen);
                if(r > 0)
                {
                    addrlen = sizeof(struct sockaddr_storage);
                    getsockname(fds[i].fd, (struct sockaddr*)&local, &addrlen);
                    r = stun_agent_input(ctx.stun, STUN_PROTOCOL_UDP, (const struct sockaddr *)&local, (const struct sockaddr *)&from, data, r);
                    assert(0 == r);
                }
            }
            else if(i > 3 + ctx.udprelays.size() && i < 3 + ctx.udprelays.size() + ctx.tcpclients.size())
            {
                // stun/turn protocol
                struct sockaddr_storage local;
                addrlen = sizeof(struct sockaddr_storage);
                TSockets::iterator it = ctx.tcpclients.find(fds[i].fd);
                assert(it != ctx.tcpclients.end());
                r = socket_recv(fds[i].fd, data, sizeof(data), 0);
                if(r > 0)
                {
                    addrlen = sizeof(struct sockaddr_storage);
                    getsockname(fds[i].fd, (struct sockaddr*)&local, &addrlen);
                    r = stun_agent_input(ctx.stun, STUN_PROTOCOL_TCP, (const struct sockaddr *)&local, (const struct sockaddr *)&(it->second), data, r);
                    assert(0 == r);
                }
            }
            else if(i > 3 + ctx.udprelays.size() + ctx.tcpclients.size() && i < 3 + ctx.udprelays.size() + ctx.tcpclients.size() + ctx.tlsclients.size())
            {
                // stun/turn protocol
                struct sockaddr_storage local;
                addrlen = sizeof(struct sockaddr_storage);
                TLSSockets::iterator it;
                for(it = ctx.tlsclients.begin(); it != ctx.tlsclients.end(); it++)
                {
                    if(tls_socket_getfd(it->first) == fds[i].fd)
                        break;
                }
                    
                assert(it != ctx.tlsclients.end());
                r = tls_socket_read(it->first, data, sizeof(data));
                if(r > 0)
                {
                    addrlen = sizeof(struct sockaddr_storage);
                    getsockname(fds[i].fd, (struct sockaddr*)&local, &addrlen);
                    r = stun_agent_input(ctx.stun, STUN_PROTOCOL_TLS, (const struct sockaddr *)&local, (const struct sockaddr *)&(it->second), data, r);
                    assert(0 == r);
                }
            }
            else
            {
                assert(0);
            }
        }
    }
    
    stun_agent_destroy(&ctx.stun);
    // TODO: close udprelays/tcpclients/tlsclient
    socket_close(ctx.tls);
    socket_close(ctx.tcp);
    socket_close(ctx.udp);
    tls_socket_cleanup();
    socket_cleanup();
}

// In all cases, the server SHOULD only allocate ports from the range 49152 - 65535
static int rtp_socket_create(struct sockaddr_in* addr, socket_t rtp[2], unsigned short port[2])
{
    unsigned short i;
    socket_t sock[2];
    srand((unsigned int)time(NULL));
    
    do
    {
        i = rand() % 10000;
        i = i/2*2 + 49152;
        
        addr->sin_port = htons(i);
        sock[0] = socket_bind_addr((const struct sockaddr*)addr, SOCK_DGRAM, 0, 1);
        if(socket_invalid == sock[0])
            continue;
        
        addr->sin_port = htons(i + 1);
        sock[1] = socket_bind_addr((const struct sockaddr*)addr, SOCK_DGRAM, 0, 1);
        if(socket_invalid == sock[1])
        {
            socket_close(sock[0]);
            continue;
        }
        
        rtp[0] = sock[0];
        rtp[1] = sock[1];
        port[0] = i;
        port[1] = i+1;
        return 0;
        
    } while(socket_invalid==sock[0] || socket_invalid==sock[1]);
    
    return -1;
}
