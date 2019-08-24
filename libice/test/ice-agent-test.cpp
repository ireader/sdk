#include "sockutil.h"
#include "ice-agent.h"
#include "stun-agent.h"
#include "stun-proto.h"
#include "port/network.h"
#include "sys/system.h"
#include "sys/pollfd.h"
#include "aio-timeout.h"
#include "base64.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <map>
#include <list>
#include <string>
#include <vector>

#define SDP_MODE 1
#ifdef SDP_MODE
#include "rtsp-media.h"
#endif

//#define STUN_SERVER "numb.viagenie.ca"
#define STUN_SERVER "stun.linphone.org"
//#define STUN_SERVER "10.95.49.51"

typedef std::map<socket_t, struct sockaddr_storage> TICESockets;

struct ice_agent_test_t
{
	int foundation;

	struct ice_agent_t* ice;
	char usr[8];
	char pwd[64];
	TICESockets udps;
};

// candidate-attribute = "candidate" ":" foundation SP component-id SP transport SP priority SP connection-address SP port SP cand-type [SP rel-addr] [SP rel-port] *(SP extension-att-name SP extension-att-value)
// a=candidate:1 1 UDP 2130706431 $L-PRIV-1.IP $L-PRIV-1.PORT typ host
// a=candidate:2 1 UDP 1694498815 $NAT-PUB-1.IP $NAT-PUB-1.PORT typ srflx raddr $L-PRIV-1.IP rport $L-PRIV-1.PORT
static int ice_candidate_attribute(const struct ice_candidate_t* c, char* buf, int len)
{
	static const char* s_transport[] = { "UDP", "TCP", "TLS", "DTLS", };
	char addr[SOCKET_ADDRLEN], raddr[SOCKET_ADDRLEN];
	u_short addrport, raddrport;
	const struct sockaddr* connaddr = (const struct sockaddr*)&c->addr;
	const struct sockaddr* realaddr = (const struct sockaddr*)ice_candidate_realaddr(c);
	assert(c->protocol <= 0 && c->protocol < sizeof(s_transport) / sizeof(s_transport[0]));
	socket_addr_to(connaddr, socket_addr_len(connaddr), addr, &addrport);
	socket_addr_to(realaddr, socket_addr_len(realaddr), raddr, &raddrport);
	if(ICE_CANDIDATE_HOST == c->type)
		return snprintf(buf, len, "%s %hu %s %u %s %hu typ host", c->foundation, c->component, s_transport[c->protocol], c->priority, addr, addrport);
	else
		return snprintf(buf, len, "%s %hu %s %u %s %hu typ %s raddr %s rport %hu", c->foundation, c->component, s_transport[c->protocol], c->priority, addr, addrport, ice_candidate_typename(c), raddr, raddrport);
}

static void ice_agent_test_ondata(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	struct ice_agent_test_t* ctx = (struct ice_agent_test_t*)param;
	/// TODO: check data format(stun/turn)
	assert(0 == ice_agent_input(ctx->ice, protocol, local, remote, data, bytes));
}

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
				assert(r == bytes || socket_geterror() == ENETUNREACH || socket_geterror() == 10051/*WSAENETUNREACH*/);
				return r == bytes ? 0 : socket_geterror();
			}
		}

		assert(0);
		//// stun/turn protocol
		//socket_t udp = socket_udp();
		//struct sockaddr_storage addr;
		//memcpy(&addr, local, socket_addr_len(local));
		//assert(AF_INET == remote->sa_family || AF_INET6 == remote->sa_family);
		//assert(AF_INET == local->sa_family || AF_INET6 == local->sa_family);
		//assert(0 == socket_bind(udp, local, socket_addr_len(local)));
		//ctx->udps.insert(std::make_pair(udp, addr));
		//int r = socket_sendto(udp, data, bytes, 0, remote, socket_addr_len(remote));
		//assert(r == bytes);
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

static void ice_agent_test_onconnected(void* param, int code)
{
	struct ice_agent_test_t* ctx = (struct ice_agent_test_t*)param;
	printf("ice connected: %d\n", code);
}

static void ice_agent_test_gather_local_candidates(void* param, const char* mac, const char* name, int /*dhcp*/, const char* ip, const char* netmask, const char* gateway)
{
	struct ice_agent_test_t* ctx = (struct ice_agent_test_t*)param;
	if (NULL == ip || 0 == *ip || 0 == strcmp("0.0.0.0", ip))
		return; // ignore
	//printf("gather local candidate: name: %s, mac: %s, ip: %s, netmask: %s, gateway: %s\n", name, mac, ip, netmask, gateway);

	++ctx->foundation;
	for (uint8_t stream = 0; stream < 1; stream++)
	{
		for (uint16_t componet = 0; componet < 1; componet++)
		{
			struct ice_candidate_t c;
			memset(&c, 0, sizeof(c));
			c.type = ICE_CANDIDATE_HOST;
			c.stream = stream;
			c.protocol = STUN_PROTOCOL_UDP;
			c.component = componet + 1;
			snprintf((char*)c.foundation, sizeof(c.foundation), "%d", ctx->foundation);

			socklen_t addrlen = sizeof(c.host);
			socket_addr_from(&c.host, &addrlen, ip, 0);
			socket_addr_from(&c.addr, &addrlen, ip, 0);
			socket_t udp = socket_bind_addr((struct sockaddr*)&c.host, SOCK_DGRAM);
			assert(socket_invalid != udp);
			addrlen = sizeof(c.host);
			getsockname(udp, (struct sockaddr*)&c.host, &addrlen);
			assert(ctx->udps.insert(std::make_pair(udp, c.host)).second);

			ice_candidate_priority(&c);
			assert(0 == ice_agent_add_local_candidate(ctx->ice, &c));

			char attr[128] = { 0 };
			ice_candidate_attribute(&c, attr, sizeof(attr));
			printf("candidate[%u]: %s\n", (unsigned int)udp, attr);
		}
	}
}

static int ice_agent_test_on_local_candidate(const struct ice_candidate_t* c, const void* param)
{
	std::vector<struct ice_candidate_t>* v = (std::vector<struct ice_candidate_t>*)param;
	v->push_back(*c);
	return 0;
}

static void ice_agent_test_print_local_candidates(struct ice_agent_test_t *ctx)
{
	std::vector<struct ice_candidate_t> v;
	std::vector<struct ice_candidate_t>::const_iterator it;
	ice_agent_list_local_candidate(ctx->ice, ice_agent_test_on_local_candidate, &v);

	char buffer[2 * 1024];
	static const char* s_transport[] = { "UDP", "TCP", "TLS", "DTLS", };
	static const char* s_candtype[] = { "host", "srflx", "prflx", "relay", };
	char addr[SOCKET_ADDRLEN];
	u_short addrport;
	int candtype;

#if !SDP_MODE
	int n = snprintf(buffer, sizeof(buffer), "%s %s", ctx->usr, ctx->pwd);
	for (it = v.begin(); it != v.end(); ++it)
	{
		const struct ice_candidate_t* c = &*it;
		const struct sockaddr* connaddr = (const struct sockaddr*)&c->addr;
		assert(c->protocol <= 0 && c->protocol < sizeof(s_transport) / sizeof(s_transport[0]));
		candtype = ICE_CANDIDATE_HOST == c->type ? 0 : (ICE_CANDIDATE_SERVER_REFLEXIVE == c->type ? 1 : (ICE_CANDIDATE_RELAYED == c->type ? 3 : 2));
		socket_addr_to(connaddr, socket_addr_len(connaddr), addr, &addrport);
		n += snprintf(buffer + n, sizeof(buffer)-n, " %s,%u,%s,%hu,%s", c->foundation, c->priority, addr, addrport, s_candtype[candtype]);
	}
	printf("Copy this line to remote client:\n\n  %s\n\n", buffer);
#else
	u_short raddrport;
	char raddr[SOCKET_ADDRLEN];
	struct ice_candidate_t default;
	ice_agent_get_candidate(ctx->ice, 0, 1, &default);
	socket_addr_to((sockaddr*)&default.addr, socket_addr_len((sockaddr*)&default.addr), addr, &addrport);

	int n = snprintf(buffer, sizeof(buffer), "m=text %hu ICE/SDP\n", addrport);
	n += snprintf(buffer + n, sizeof(buffer) - n, "c=IN IP4 %s\n", addr);
	n += snprintf(buffer + n, sizeof(buffer) - n, "a=ice-ufrag:%s\n", ctx->usr);
	n += snprintf(buffer + n, sizeof(buffer) - n, "a=ice-pwd:%s\n", ctx->pwd);
	for (it = v.begin(); it != v.end(); ++it)
	{
		const struct ice_candidate_t* c = &*it;
		const struct sockaddr* connaddr = (const struct sockaddr*)&c->addr;
		const struct sockaddr* realaddr = (const struct sockaddr*)ice_candidate_realaddr(c);
		assert(c->protocol <= 0 && c->protocol < sizeof(s_transport) / sizeof(s_transport[0]));
		candtype = ICE_CANDIDATE_HOST == c->type ? 0 : (ICE_CANDIDATE_SERVER_REFLEXIVE == c->type ? 1 : (ICE_CANDIDATE_RELAYED == c->type ? 3 : 2));
		socket_addr_to(connaddr, socket_addr_len(connaddr), addr, &addrport);
		socket_addr_to(realaddr, socket_addr_len(realaddr), raddr, &raddrport);
		if (ICE_CANDIDATE_HOST == c->type)
			n += snprintf(buffer + n, sizeof(buffer) - n, "a=candidate:%s %hu %s %u %s %hu typ host\n", c->foundation, c->component, s_transport[c->protocol], c->priority, addr, addrport);
		else
			n += snprintf(buffer + n, sizeof(buffer) - n, "a=candidate:%s %hu %s %u %s %hu typ %s raddr %s rport %hu\n", c->foundation, c->component, s_transport[c->protocol], c->priority, addr, addrport, ice_candidate_typename(c), raddr, raddrport);
	}

	char base64[3 * 1024];
	n = base64_encode(base64, buffer, n);
	printf("Copy this line to remote client:\n\n  %.*s\n\n", n, base64);
#endif
}

static enum ice_candidate_type_t ice_candidate_type(const char* candtype)
{
	int i;
	static const char* s_candtype[] = { "host", "srflx", "prflx", "relay", };
	static const enum ice_candidate_type_t s_value[] = { ICE_CANDIDATE_HOST, ICE_CANDIDATE_SERVER_REFLEXIVE, ICE_CANDIDATE_PEER_REFLEXIVE, ICE_CANDIDATE_RELAYED };
	for (i = 0; i < sizeof(s_candtype) / sizeof(s_candtype[0]); i++)
	{
		if (0 == strcmp(s_candtype[i], candtype))
			return s_value[i];
	}
	return ICE_CANDIDATE_RELAYED;
}
static int ice_candidate_transport(const char* transport)
{
	int i;
	static const char* s_transport[] = { "UDP", "TCP", "TLS", "DTLS" };
	static const int s_value[] = { STUN_PROTOCOL_UDP, STUN_PROTOCOL_TCP, STUN_PROTOCOL_TLS, STUN_PROTOCOL_DTLS };
	for (i = 0; i < sizeof(s_transport) / sizeof(s_transport[0]); i++)
	{
		if (0 == strcmp(s_transport[i], transport))
			return s_value[i];
	}
	return STUN_PROTOCOL_UDP;
}

static void ice_agent_test_gather_remote_candidates(struct ice_agent_test_t *ctx)
{
	char buffer[2 * 1024];
	printf("Enter remote data (single line, no wrapping)\n>");
	fgets(buffer, sizeof(buffer), stdin);

#if !SDP_MODE
	char usr[64];
	char pwd[512];
	int n, offset = 0;
	assert(2 == sscanf(buffer, "%64s %s%n", usr, pwd, &n));
	ice_agent_set_remote_auth(ctx->ice, usr, pwd);

	offset += n;
	u_short addrport;
	char addr[SOCKET_ADDRLEN];
	char candtype[16];
	struct ice_candidate_t c;
	memset(&c, 0, sizeof(c));

	while (5 == sscanf(buffer + offset, " %32[^,\r\n ],%u,%[^,\r\n ],%hu,%15s%n", c.foundation, &c.priority, &addr, &addrport, candtype, &n))
	{
		c.type = ice_candidate_type(candtype);
		c.stream = 0;
		c.protocol = STUN_PROTOCOL_UDP;
		c.component = 1;
		
		socklen_t addrlen = sizeof(c.host);
		socket_addr_from(&c.host, &addrlen, addr, addrport);
		socket_addr_from(&c.addr, &addrlen, addr, addrport);
		assert(0 == ice_agent_add_remote_candidate(ctx->ice, &c));

		offset += n;
		memset(&c, 0, sizeof(c));
	}
#else
	char sdp[2 * 1024];
	int n = strlen(buffer);
	assert(n > 0);
	n = base64_decode(sdp, buffer, buffer[n-1]=='\n' ? n-1 : n);
	sdp[n] = 0;
	struct rtsp_media_t m[2];
	assert(1 == rtsp_media_sdp(sdp, m, 2));
	for (int i = 0; i < m[0].ice.ncandidate; i++)
	{
		struct ice_candidate_t c;
		memset(&c, 0, sizeof(c));
		c.stream = 0;
		c.priority = m[0].ice.candidates[i].priority;
		c.component = m[0].ice.candidates[i].component;
		c.type = ice_candidate_type(m[0].ice.candidates[i].candtype);
		c.protocol = ice_candidate_transport(m[0].ice.candidates[i].transport);
		snprintf(c.foundation, sizeof(c.foundation), "%s", m[0].ice.candidates[i].foundation);
		
		socklen_t addrlen = sizeof(c.host);
		socket_addr_from(&c.addr, &addrlen, m[0].ice.candidates[i].address, m[0].ice.candidates[i].port);
		memcpy(&c.host, &c.addr, sizeof(c.host));
		assert(0 == ice_agent_add_remote_candidate(ctx->ice, &c));
	}
#endif
}

static void ice_agent_test_ongather(void* param, int code)
{
	assert(0 == code);
	struct ice_agent_test_t* ctx = (struct ice_agent_test_t*)param;
	ice_agent_test_print_local_candidates(ctx);
	ice_agent_test_gather_remote_candidates(ctx);
	assert(0 == ice_agent_start(ctx->ice));
}

extern "C" void ice_agent_test(void)
{
	uint8_t data[2000];
	int r, timeout = 10;
	
	socket_init();

	struct ice_agent_test_t ctx;
	struct ice_agent_handler_t handler;
	
	memset(&handler, 0, sizeof(handler));
	handler.send = ice_agent_test_send;
	handler.ondata = ice_agent_test_ondata;
	handler.ongather = ice_agent_test_ongather;
	handler.onconnected = ice_agent_test_onconnected;
	ctx.ice = ice_agent_create(1, &handler, &ctx);
	ctx.foundation = 0;
	strcpy(ctx.usr, "demo");
	strcpy(ctx.pwd, "demodemodemodemodemo");
	ice_agent_set_local_auth(ctx.ice, ctx.usr, ctx.pwd);

	struct sockaddr_storage stun;
	socklen_t len = sizeof(stun);
	//assert(0 == socket_addr_from(&stun, &len, "numb.viagenie.ca", STUN_PORT));
	assert(0 == socket_addr_from(&stun, &len, STUN_SERVER, STUN_PORT));
	network_getip(ice_agent_test_gather_local_candidates, &ctx);
	ice_agent_gather(ctx.ice, (const sockaddr*)&stun, 0, 2000);

	while (1)
	{
		aio_timeout_process();

		int n = 0;
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
					r = ice_agent_input(ctx.ice, STUN_PROTOCOL_UDP, (const struct sockaddr *)&it->second, (const struct sockaddr *)&from, data, r);
					assert(0 == r);
				}
				break;
			}
			assert(it != ctx.udps.end());
		}
	}

	ice_agent_destroy(ctx.ice);
	socket_cleanup();
}

void* stun_timer_start(int ms, void(*ontimer)(void* param), void* param)
{
	aio_timeout_t* ptr = (aio_timeout_t*)calloc(1, sizeof(aio_timeout_t));
	if (0 == aio_timeout_start(ptr, ms, ontimer, param))
		return ptr;
	free(ptr);
	return NULL;
}

int stun_timer_stop(void* timer)
{
	return aio_timeout_stop((aio_timeout_t*)timer);
}
