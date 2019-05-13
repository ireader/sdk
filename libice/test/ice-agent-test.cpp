#include "ice-agent.h"
#include "stun-agent.h"
#include "stun-proto.h"
#include "port/network.h"
#include "sys/system.h"
#include "sockutil.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <map>

typedef std::map<socket_t, struct sockaddr_storage> TICESockets;

struct ice_agent_test_t
{
	struct ice_agent_t* ice;
	TICESockets udps;
};

static int ice_agent_test_send(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	struct ice_agent_test_t* ctx = (struct ice_agent_test_t*)param;

	if (STUN_PROTOCOL_UDP == protocol)
	{
		TICESockets::iterator it;
		for (it = ctx->udps.begin(); it != ctx->udps.end(); it++)
		{
			if (0 == socket_addr_compare((const struct sockaddr*)&it->second, local))
			{
				int r = socket_sendto(it->first, data, bytes, 0, remote, socket_addr_len(remote));
				assert(r == bytes);
				return 0;
			}
		}

		// stun/turn protocol
		socket_t udp = socket_udp();
		struct sockaddr_storage addr;
		memcpy(&addr, local, socket_addr_len(local));
		assert(AF_INET == remote->sa_family || AF_INET6 == remote->sa_family);
		assert(AF_INET == local->sa_family || AF_INET6 == local->sa_family);
		assert(0 == socket_bind(udp, local, socket_addr_len(local)));
		ctx->udps.insert(std::make_pair(udp, addr));
		int r = socket_sendto(udp, data, bytes, 0, remote, socket_addr_len(remote));
		assert(r == bytes);
	}
	else if (STUN_PROTOCOL_TCP == protocol)
	{
		assert(0);
	}
	else if (STUN_PROTOCOL_TLS == protocol)
	{
		assert(0);
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

static int ice_agent_test_auth(void* param, const char* usr, char pwd[256])
{
	return 0;
}

static void ice_agent_test_gather_local_candidates(void* param, const char* mac, const char* name, int /*dhcp*/, const char* ip, const char* netmask, const char* gateway)
{
	struct ice_agent_test_t* ctx = (struct ice_agent_test_t*)param;
	if (NULL == ip || 0 == *ip || 0 == strcmp("0.0.0.0", ip))
		return; // ignore
	printf("gather local candidate: name: %s, mac: %s, ip: %s, netmask: %s, gateway: %s\n", name, mac, ip, netmask, gateway);

	static int foundation = 0;
	foundation++;

	struct sockaddr_storage stun;
	memset(&stun, 0, sizeof(stun));
	assert(0 == socket_addr_from_ipv4((struct sockaddr_in*)&stun, "10.224.9.246", STUN_PORT));

	for (int stream = 0; stream < 2; stream++)
	{
		struct ice_candidate_t c;
		memset(&c, 0, sizeof(c));
		c.type = ICE_CANDIDATE_HOST;
		c.protocol = STUN_PROTOCOL_UDP;
		c.component = stream + 1;
		snprintf((char*)c.foundation, sizeof(c.foundation), "%d", foundation);

		socklen_t addrlen = sizeof(c.addr);
		socket_addr_from(&c.addr, &addrlen, ip, 0);
		socket_t udp = socket_bind_addr((struct sockaddr*)&c.addr, SOCK_DGRAM);
		assert(socket_invalid != udp);
		addrlen = sizeof(c.addr);
		getsockname(udp, (struct sockaddr*)&c.addr, &addrlen);
		ctx->udps.insert(std::make_pair(udp, c.addr));

		ice_candidate_priority(&c);
		memcpy(&c.stun, &stun, sizeof(c.stun));
		memcpy(&c.base, &c.addr, sizeof(c.base));
		assert(0 == ice_add_local_candidate(ctx->ice, stream, &c));
	}
}

static void ice_agent_test_gather_remote_candidates(struct ice_agent_test_t *ctx)
{
}

static int ice_agent_test_ongather(void* param, int code)
{
	assert(0 == code);
	struct ice_agent_test_t* ctx = (struct ice_agent_test_t*)param;
	ice_agent_test_gather_remote_candidates(ctx);
	assert(0 == ice_start(ctx->ice));
	return code;
}

extern "C" void ice_agent_test(void)
{
	uint8_t data[2000];
	int r, timeout = 5000;
	
	socket_init();

	struct ice_agent_test_t ctx;
	struct ice_agent_handler_t handler;
	
	memset(&handler, 0, sizeof(handler));
	handler.send = ice_agent_test_send;
	handler.auth = ice_agent_test_auth;
	ctx.ice = ice_create(&handler, &ctx);

	network_getip(ice_agent_test_gather_local_candidates, &ctx);
	ice_gather_stun_candidate(ctx.ice, "numb.viagenie.ca", 1, ice_agent_test_ongather, &ctx);

	while (1)
	{
#if defined(OS_WINDOWS)
		fd_set fds;
		struct timeval tv;
		FD_ZERO(&fds);
		for (TICESockets::const_iterator it = ctx.udps.begin(); it != ctx.udps.end(); ++it)
			FD_SET(it->first, &fds);

		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		r = socket_select_readfds(0, &fds, timeout < 0 ? NULL : &tv);
		if (r < 0)
		{
			printf("poll error: %d, errno: %d\n", r, errno);
		}
		else if (0 == r)
		{
			continue; // timeout
		}

		for (TICESockets::iterator it = ctx.udps.begin(); it != ctx.udps.end(); ++it)
		{
			if (!FD_ISSET(it->first, &fds))
				continue;

			sockaddr_storage from;
			socklen_t addrlen = sizeof(struct sockaddr_storage);
			r = socket_recvfrom(it->first, data, sizeof(data), 0, (struct sockaddr*)&from, &addrlen);
			if (r > 0)
			{
				r = ice_input(ctx.ice, STUN_PROTOCOL_UDP, (const struct sockaddr *)&it->second, (const struct sockaddr *)&from, data, r);
				assert(0 == r);
			}
		}
#else
		struct pollfd fds[64];
		for (TICESockets::const_iterator it = ctx.udps.begin(); it != ctx.udps.end(); ++it)
		{
			fds[n].fd = it->first;
			fds[n].events = POLLIN;
			fds[n].revents = 0;
			n++;
		}

		r = poll(fds, n, timeout);
		while (-1 == r && EINTR == errno)
			r = poll(fds, n, timeout);

		if (r < 0)
		{
			printf("poll error: %d, errno: %d\n", r, errno);
		}
		else if (0 == r)
		{
			continue; // timeout
		}

		for (int i = 0; i < n; i++)
		{
			if (0 == (fds[i].revents & POLLIN))
				continue;

			TICESockets::iterator it;
			for (it = ctx.udps.begin(); it != ctx.udps.end(); it++)
			{
				if (it->first != fds[i].fd)
					continue;

				sockaddr_storage from;
				socklen_t addrlen = sizeof(struct sockaddr_storage);
				r = socket_recvfrom(it->first, data, sizeof(data), 0, (struct sockaddr*)&from, &addrlen);
				if (r > 0)
				{
					r = ice_input(ctx.ice, STUN_PROTOCOL_UDP, (const struct sockaddr *)&it->second, (const struct sockaddr *)&from, data, r);
					assert(0 == r);
				}
				break;
			}
			assert(it != ctx.udps.end());
		}
#endif
	}

	ice_destroy(ctx.ice);
	socket_cleanup();
}
