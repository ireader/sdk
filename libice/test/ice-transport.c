#include "sockutil.h"
#include "ice-transport.h"
#include "stun-proto.h"
#include "stun-agent.h"
#include "ice-agent.h"
#include "sys/thread.h"
#include "sys/locker.h"
#include "sys/system.h"
#include "port/network.h"
#include "port/ip-route.h"
#include "aio-timeout.h"
#include "sockpair.h"
#include "base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define ICE_LOCAL_ENABLE 0x01
#define ICE_REMOTE_ENABLE 0x02

int read_random(void* ptr, int bytes);

static int STDCALL ice_transport_thread(void* param);

struct ice_transport_t
{
	struct ice_agent_t* ice;
	char usr[16];
	char pwd[48];
	
	socket_t udp[64];
	struct sockaddr_storage addr[64];
	int naddr;
	int stream;
	int component;
	int foundation;
	uint64_t connected;

	int ipv6; // TODO: enable IPv6
	int stun;
	int running;
	pthread_t thread;
	char ptr[4 * 1024];

	struct ice_transport_handler_t handler;
	void* param;
};

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

static inline const char* ice_candidate_transport_name(int protocol)
{
	static const char* s_transport[] = { "UDP", "TCP", "TLS", "DTLS" };
	assert(protocol <= 0 && protocol < sizeof(s_transport) / sizeof(s_transport[0]));
	return s_transport[protocol % (sizeof(s_transport) / sizeof(s_transport[0]))];
}

static enum ice_candidate_type_t ice_candidate_name2type(const char* candtype)
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
	if (ICE_CANDIDATE_HOST == c->type)
		return snprintf(buf, len, "a=candidate:%s %hu %s %u %s %hu typ host\n", c->foundation, c->component, ice_candidate_transport_name(c->protocol), c->priority, addr, addrport);
	else
		return snprintf(buf, len, "a=candidate:%s %hu %s %u %s %hu typ %s raddr %s rport %hu\n", c->foundation, c->component, ice_candidate_transport_name(c->protocol), c->priority, addr, addrport, ice_candidate_typename(c), raddr, raddrport);
}

static void ice_agent_username(char usr[16], char pwd[48])
{
	int n;
	char ptr[40];

	read_random(ptr, sizeof(ptr));
	n = base64_encode_url(usr, ptr, 6);
	usr[n] = 0;
	n = base64_encode_url(pwd, ptr+9, 18);
	pwd[n] = 0;
}

static void ice_transport_ondata(void* param, uint8_t stream, uint16_t component, const void* data, int bytes)
{
	struct ice_transport_t* avt = (struct ice_transport_t*)param;
	avt->handler.ondata(avt->param, stream, component, data, bytes);
}

static inline u_short socket_addr_getport(const struct sockaddr* addr)
{
	switch (addr->sa_family)
	{
	case AF_INET:	return ntohs(((struct sockaddr_in*)addr)->sin_port);
	case AF_INET6:	return ntohs(((struct sockaddr_in6*)addr)->sin6_port);
	default: return 0;
	}
}

static int ice_transport_onsend(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	int i, r;
	socket_bufvec_t vec[1];
	struct ice_transport_t* avt = (struct ice_transport_t*)param;
	assert(STUN_PROTOCOL_UDP == protocol);
	for (i = 0; i < avt->naddr; i++)
	{
		//if (0 == socket_addr_compare((struct sockaddr*)&avt->addr[i], local))
		if(local->sa_family == avt->addr[i].ss_family && socket_addr_getport((struct sockaddr*)&avt->addr[i]) == socket_addr_getport(local))
		{
			socket_setbufvec(vec, 0, (void*)data, bytes);
			if(avt->stun & ICE_LOCAL_ENABLE)
				r = socket_sendto_addr(avt->udp[i], vec, sizeof(vec) / sizeof(vec[0]), 0, remote, socket_addr_len(remote), local, socket_addr_len(local));
			else
				r = socket_sendto(avt->udp[i], data, bytes, 0, remote, socket_addr_len(remote));
			assert(r == bytes || socket_geterror() == ENETUNREACH || socket_geterror() == 10051/*WSAENETUNREACH*/);
			return r == bytes ? 0 : socket_geterror();
		}
	}

	return -1;
}

static void ice_transport_ongather(void* param, int code)
{
	struct ice_transport_t* avt = (struct ice_transport_t*)param;
	avt->handler.onbind(avt->param, code);
}

static void ice_transport_onconnected(void* param, uint64_t flags, uint64_t mask)
{
	struct ice_transport_t* avt = (struct ice_transport_t*)param;
	avt->connected = flags;
	avt->handler.onconnected(avt->param, flags, mask);
	//TODO: close other socket
}

struct ice_transport_t* ice_transport_create(int controlling, struct ice_transport_handler_t* h, void* param)
{
	struct ice_transport_t* avt;
	struct ice_agent_handler_t handler;

	avt = (struct ice_transport_t*)calloc(1, sizeof(*avt));
	if (avt)
	{
		avt->param = param;
		ice_agent_username(avt->usr, avt->pwd);
		memset(avt->udp, -1, sizeof(avt->udp));
		memset(&handler, 0, sizeof(handler));
		memcpy(&avt->handler, h, sizeof(avt->handler));
		handler.send = ice_transport_onsend;
		handler.ondata = ice_transport_ondata;
		handler.ongather = ice_transport_ongather;
		handler.onconnected = ice_transport_onconnected;
		avt->ice = ice_agent_create(controlling, &handler, avt);
		ice_agent_set_local_auth(avt->ice, avt->usr, avt->pwd);
	}

	return avt;
}

int ice_transport_destroy(struct ice_transport_t* avt)
{
	if (avt->ice)
	{
		ice_agent_destroy(avt->ice);
		avt->ice = NULL;
	}

	free(avt);
	return 0;
}

static int ice_add_local_candidate(struct ice_agent_t* ice, int stream, int component, int foundation, const struct sockaddr* addr)
{
	int r;
	struct ice_candidate_t c;
	
	memset(&c, 0, sizeof(c));
	c.stream = (uint8_t)stream;
	c.component = (uint16_t)component;
	c.type = ICE_CANDIDATE_HOST;
	c.protocol = STUN_PROTOCOL_UDP;
	memcpy(&c.host, addr, socket_addr_len(addr));
	memcpy(&c.addr, addr, socket_addr_len(addr));

	ice_candidate_priority(&c);
	snprintf((char*)c.foundation, sizeof(c.foundation), "%d", foundation);
	r = ice_agent_add_local_candidate(ice, &c);

	//char attr[128] = { 0 };
	//ice_candidate_attribute(&c, attr, sizeof(attr));
	//printf("add local candidate: %s\n", attr);
	return r;
}

static int ice_add_remote_candidates(struct ice_agent_t* ice, const struct rtsp_media_t* avmedia, int count)
{
	int r, i, j, total;
	socklen_t len;
	struct ice_candidate_t c;
	const struct rtsp_media_t *m;

	for (total = i = 0; i < count; i++)
	{
		m = avmedia + i;
		for (j = 0; j < m->ice.candidate_count; j++)
		{
			memset(&c, 0, sizeof(c));
			c.stream = (uint8_t)i;
			c.priority = m->ice.candidates[j]->priority;
			c.component = m->ice.candidates[j]->component;
			c.protocol = ice_candidate_transport(m->ice.candidates[j]->transport);
			c.type = ice_candidate_name2type(m->ice.candidates[j]->candtype);
			snprintf(c.foundation, sizeof(c.foundation), "%s", m->ice.candidates[j]->foundation);
			socket_addr_from(&c.addr, &len, m->ice.candidates[j]->address, m->ice.candidates[j]->port);
			memcpy(&c.host, &c.addr, sizeof(struct sockaddr_storage)); // remote candidate c.host = c.addr
			r = ice_agent_add_remote_candidate(ice, &c);
			//assert(0 == r);
			if(0 == r)
				total += 1;

			//char attr[128] = { 0 };
			//ice_candidate_attribute(&c, attr, sizeof(attr));
			//printf("add remote candidate: %s\n", attr);
		}

		ice_agent_set_remote_auth(ice, i, avmedia[i].ice.ufrag, avmedia[i].ice.pwd);
	}

	return total;
}

struct ice_transport_list_local_candidates_t
{
	int stream;
	char* ptr;
	char* end;
};

static int ice_transport_candidate_onsdp(const struct ice_candidate_t* c, void* param)
{
	int n;
	struct ice_transport_list_local_candidates_t* s;
	s = (struct ice_transport_list_local_candidates_t*)param;
	if (c->stream != s->stream)
		return 0;

	n = ice_candidate_attribute(c, s->ptr, s->end - s->ptr);
	if (n < 0 || n >= s->end - s->ptr)
		return -1;
	s->ptr += n;
	return 0;
}

int ice_transport_getsdp(struct ice_transport_t* avt, int stream, char* buf, int bytes)
{
	static const char* pattern = "a=ice-ufrag:%s\na=ice-pwd:%s\nc=IN %s %s\na=rtcp:%hu\na=sendrecv\n";

	int i, n;
	u_short port;
	char host[SOCKET_ADDRLEN];
	struct ice_candidate_t c;
	struct ice_transport_list_local_candidates_t p;
	p.stream = stream;
	p.ptr = buf;
	p.end = buf + bytes;

	if (avt->stun & ICE_LOCAL_ENABLE)
	{
		// ice default address
		memset(&c, 0, sizeof(c));
		ice_agent_get_local_candidate(avt->ice, stream, 1, &c);
		socket_addr_to((struct sockaddr*)&c.addr, socket_addr_len((struct sockaddr*)&c.addr), host, &port);
		n = snprintf(p.ptr, p.end - p.ptr, pattern, avt->usr, avt->pwd, AF_INET6==c.addr.ss_family ? "IP6" : "IP4", host, port);
		if (n < 0 || p.ptr + n >= p.end)
			return -1;
		p.ptr += n;

		if (avt->connected)
		{
			// local candidates
			for (i = 0; i < avt->component; i++)
			{
				memset(&c, 0, sizeof(c));
				ice_agent_get_local_candidate(avt->ice, stream, i + 1, &c);
				ice_transport_candidate_onsdp(&c, &p);
			}

			// remote candidates
			n = snprintf(p.ptr, p.end-p.ptr, "a=remote-candidates:");
			if (n < 0 || p.ptr + n >= p.end)
				return -1;
			p.ptr += n;

			for (i = 0; i < avt->component; i++)
			{
				memset(&c, 0, sizeof(c));
				ice_agent_get_remote_candidate(avt->ice, stream, i + 1, &c);
				socket_addr_to((struct sockaddr*)&c.addr, socket_addr_len((struct sockaddr*)&c.addr), host, &port);
				n = snprintf(p.ptr, p.end - p.ptr, " %d %s %hu", i + 1, host, port);
				if (n < 0 || p.ptr + n >= p.end)
					return -1;
				p.ptr += n;
			}
		}
		else
		{
			if (0 != ice_agent_list_local_candidate(avt->ice, ice_transport_candidate_onsdp, &p))
				return 0;
		}
	}

	return (int)(p.ptr - buf);
}

static void ice_transport_gather_local_candidate_handler(void* param, const char* mac, const char* name, int dhcp, const char* ip, const char* netmask, const char* gateway)
{
	int i, j, n;
	socklen_t addrlen;
	struct sockaddr_storage addr, *addr0;
	struct ice_transport_t* avt;

	(void)gateway, (void)netmask, (void)dhcp, (void)name, (void)mac;
	avt = (struct ice_transport_t*)param;

	if (NULL == ip || 0 == *ip || 0 == strcmp("0.0.0.0", ip) || 0 == strcmp("::", ip) || 0 == strcmp("127.0.0.1", ip) || 0 == strcmp("::1", ip))
		return; // ignore

	socket_addr_from(&addr, &addrlen, ip, 0);
	n = AF_INET6 == addr.ss_family ? avt->stream*avt->component : 0;

	for (i = 0; i < avt->stream; i++)
	{
		for (j = 0; j < avt->component; j++)
		{
			addr0 = &avt->addr[n + i * avt->stream + j];
			assert(addr0->ss_family == addr.ss_family);
			if (AF_INET == addr0->ss_family)
				((struct sockaddr_in*)&addr)->sin_port = ((struct sockaddr_in*)addr0)->sin_port;
			else
				((struct sockaddr_in6*)&addr)->sin6_port = ((struct sockaddr_in*)addr0)->sin_port;
			ice_add_local_candidate(avt->ice, i, j+1, avt->foundation + 1, (struct sockaddr*)&addr);
		}
	}

	++avt->foundation;
}

int ice_transport_bind(struct ice_transport_t* avt, int stream, int component, const struct sockaddr* stun, int turn, const char* usr, const char* pwd)
{
	int i, j, k, r;
	u_short port[2];
	char ip[SOCKET_ADDRLEN];
	static const char *address[] = { "0.0.0.0", "::" };
	struct sockaddr_storage addr;
	socklen_t addrlen;

	if (component > 2)
	{
		assert(0);
		return -1;
	}
	
	avt->stream = stream;
	avt->component = component;
	avt->foundation = 0;

	for (i = 0; i < sizeof(address) / sizeof(address[0]); i++)
	{
		if (avt->naddr + component >= sizeof(avt->udp) / sizeof(avt->udp[0]))
		{
			assert(0);
			break;
		}

		r = socket_addr_from(&addr, &addrlen, address[i], 0);
		if(0 != r)
			continue;

		for (j = 0; j < stream; j++)
		{
			if (2 == component)
				sockpair_create2((struct sockaddr*)&addr, &avt->udp[avt->naddr], port);
			else
				avt->udp[avt->naddr] = socket_udp_bind_addr((struct sockaddr*)&addr, 0, 0);

			for (k = 0; k < component; k++)
			{
				if(AF_INET == addr.ss_family)
					r = socket_setpktinfo(avt->udp[avt->naddr + k], 1);
				else if(AF_INET6 == addr.ss_family)
					r = socket_setpktinfo6(avt->udp[avt->naddr + k], 1);
				assert(0 == r);

				//socket_setnonblock(avt->udp[avt->naddr + k], 1);
				getsockname(avt->udp[avt->naddr + k], (struct sockaddr*)&avt->addr[avt->naddr + k], &addrlen);
			}
			avt->naddr += component;
		}
	}

	if (0 == avt->naddr)
	{
		assert(0);
		return -1;
	}

	// start run thread
	assert(0 == avt->running);
	avt->running = 1;
	thread_create(&avt->thread, ice_transport_thread, avt);

	if (stun && (AF_INET == stun->sa_family || AF_INET6 == stun->sa_family))
	{
		avt->stun |= ICE_LOCAL_ENABLE;
		network_getip(ice_transport_gather_local_candidate_handler, avt);
		return ice_agent_gather(avt->ice, (struct sockaddr*)stun, turn, 2000, STUN_CREDENTIAL_LONG_TERM, usr, pwd);
	}
	else
	{
		ip_route_get("0.0.0.0", ip); // default ip(IPv4)
		avt->stun = (avt->stun & ~ICE_LOCAL_ENABLE);
		ice_transport_gather_local_candidate_handler(avt, NULL, NULL, 0, ip, NULL, NULL);
		ice_transport_ongather(avt, 0);
		return 0;
	}
}

int ice_transport_getaddr(struct ice_transport_t* avt, int stream, int component, struct sockaddr_storage* local)
{
	struct ice_candidate_t c;

	memset(&c, 0, sizeof(c));
	if (0 != ice_agent_get_local_candidate(avt->ice, (uint8_t)stream, (uint16_t)component, &c))
		return -1;

	memcpy(local, &c.addr, sizeof(struct sockaddr_storage));
	return 0;
}

int ice_transport_connect(struct ice_transport_t* avt, const struct rtsp_media_t* avmedia, int count)
{
	int i, j, r;
	uint64_t flags, mask;
	socklen_t len;
	struct ice_candidate_t c;
	const struct rtsp_media_t *m;
	
	r = ice_add_remote_candidates(avt->ice, avmedia, count);
	avt->stun = (avt->stun & ~ICE_REMOTE_ENABLE) | (r > 0 ? ICE_REMOTE_ENABLE : 0);

	if (avt->stun & ICE_REMOTE_ENABLE)
		return ice_agent_start(avt->ice);

	// add default candidate(for non-ice mode)
	for (mask = flags = i = 0; i < count; i++)
	{
		m = avmedia + i;
		for (j = 0; j < m->nport; j++)
		{
			memset(&c, 0, sizeof(c));
			c.stream = (uint8_t)i;
			c.priority = 1;
			c.component = (uint16_t)(j + 1);
			c.protocol = STUN_PROTOCOL_UDP;
			c.type = ICE_CANDIDATE_HOST;
			snprintf(c.foundation, sizeof(c.foundation), "%s", "default");
			socket_addr_from(&c.addr, &len, m->address, (u_short)m->port[j]);
			memcpy(&c.host, &c.addr, sizeof(struct sockaddr_storage)); // remote candidate c.host = c.addr
			ice_agent_add_remote_candidate(avt->ice, &c);
		}

		flags |= (uint64_t)1 << i;
		mask |= (uint64_t)1 << i;
	}
	
	avt->handler.onconnected(avt->param, flags, mask);
	return 0;
}

int ice_transport_cancel(struct ice_transport_t* avt)
{
	return ice_agent_stop(avt->ice);
}

int ice_transport_send(struct ice_transport_t* avt, int stream, int component, const void* data, int bytes)
{
	return ice_agent_send(avt->ice, (uint8_t)stream, (uint16_t)component, data, bytes);
}

static int STDCALL ice_transport_thread(void* param)
{
	int i, n;
	int64_t r;
	struct ice_transport_t* avt;
	struct sockaddr_storage local, peer;
	socklen_t locallen, peerlen;
	socket_bufvec_t vec[1];
	
	avt = (struct ice_transport_t*)param;
	socket_setbufvec(vec, 0, avt->ptr, sizeof(avt->ptr));

	while (avt->running)
	{
		aio_timeout_process();

		r = socket_poll_read(avt->udp, avt->naddr, 1000);
		if (0 == r)
		{
			continue; // timeout
		}
		else if (r < 0)
		{
			system_sleep(10);
			continue;
			//break; // error
		}
		else
		{
			for (i = 0; i < avt->naddr; i++)
			{
				if (0 == (r & ((int64_t)1 << i)))
					continue;

				peerlen = sizeof(peer);
				locallen = sizeof(local);
				//n = socket_recvfrom(avt->udp[i], avt->ptr, sizeof(avt->ptr), 0, (struct sockaddr*)&peer, &peerlen);
				n = socket_recvfrom_addr(avt->udp[i], vec, sizeof(vec) / sizeof(vec[0]), 0, (struct sockaddr*)&peer, &peerlen, (struct sockaddr*)&local, &locallen);
				if (n <= 0)
					continue;

				socket_addr_setport((struct sockaddr*)&local, locallen, socket_addr_getport((struct sockaddr*)&avt->addr[i]));
				n = ice_agent_input(avt->ice, STUN_PROTOCOL_UDP, (struct sockaddr*)&local, (struct sockaddr*)&peer, avt->ptr, n);
				assert(0 == n);
			}
		}
	}

	return 0;
}

void* stun_timer_start(int ms, void(*ontimer)(void* param), void* param)
{
	struct aio_timeout_t* ptr;
	ptr = (struct aio_timeout_t*)calloc(1, sizeof(struct aio_timeout_t));
	if (0 == aio_timeout_start(ptr, ms, ontimer, param))
		return ptr;
	free(ptr);
	return NULL;
}

int stun_timer_stop(void* timer)
{
	return aio_timeout_stop((struct aio_timeout_t*)timer);
}
