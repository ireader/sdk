#include "sockutil.h"
#include "ice-transport.h"
#include "stun-proto.h"
#include "stun-agent.h"
#include "ice-agent.h"
#include "sys/thread.h"
#include "sys/locker.h"
#include "sys/system.h"
#include "port/network.h"
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
		return snprintf(buf, len, "%s %hu %s %u %s %hu typ host", c->foundation, c->component, ice_candidate_transport_name(c->protocol), c->priority, addr, addrport);
	else
		return snprintf(buf, len, "%s %hu %s %u %s %hu typ %s raddr %s rport %hu", c->foundation, c->component, ice_candidate_transport_name(c->protocol), c->priority, addr, addrport, ice_candidate_typename(c), raddr, raddrport);
}

static void ice_agent_username(char usr[16], char pwd[48])
{
	int n;
	char ptr[40];

	read_random(ptr, sizeof(ptr));
	n = base64_encode(usr, ptr, 6);
	usr[n] = 0;
	n = base64_encode(pwd, ptr+9, 18);
	pwd[n] = 0;
}

static void ice_transport_ondata(void* param, uint8_t stream, uint16_t component, const void* data, int bytes)
{
	struct ice_transport_t* avt = (struct ice_transport_t*)param;
	avt->handler.ondata(avt->param, stream, component, data, bytes);
}

static int ice_transport_onsend(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	int i;
	struct ice_transport_t* avt = (struct ice_transport_t*)param;
	assert(STUN_PROTOCOL_UDP == protocol);
	for (i = 0; i < sizeof(avt->udp)/sizeof(avt->udp[0]); i++)
	{
		if (0 == socket_addr_compare((struct sockaddr*)&avt->addr[i], local))
		{
			int r = socket_sendto(avt->udp[i], data, bytes, 0, remote, socket_addr_len(remote));
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

static void ice_transport_onconnected(void* param, int64_t streams)
{
	struct ice_transport_t* avt = (struct ice_transport_t*)param;
	avt->handler.onconnected(avt->param, streams);

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
	int i, j, total;
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
			if(0 == ice_agent_add_remote_candidate(ice, &c))
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

static int ice_transport_oncandidate(const struct ice_candidate_t* c, const void* param)
{
	static const char* s_transport[] = { "UDP", "TCP", "TLS", "DTLS", };

	int n;
	u_short addrport, raddrport;
	char addr[SOCKET_ADDRLEN], raddr[SOCKET_ADDRLEN];
	const struct sockaddr* connaddr;
	const struct sockaddr* realaddr;
	struct ice_transport_list_local_candidates_t* s;

	s = (struct ice_transport_list_local_candidates_t*)param;
	if (c->stream != s->stream)
		return 0;

	connaddr = (const struct sockaddr*)&c->addr;
	realaddr = (const struct sockaddr*)ice_candidate_realaddr(c);
	assert(c->protocol <= 0 && c->protocol < sizeof(s_transport) / sizeof(s_transport[0]));
	socket_addr_to(connaddr, socket_addr_len(connaddr), addr, &addrport);
	socket_addr_to(realaddr, socket_addr_len(realaddr), raddr, &raddrport);
	if (ICE_CANDIDATE_HOST == c->type)
		n = snprintf(s->ptr, s->end - s->ptr, "a=candidate:%s %hu %s %u %s %hu typ host\n", c->foundation, c->component, ice_candidate_transport_name(c->protocol), c->priority, addr, addrport);
	else
		n = snprintf(s->ptr, s->end - s->ptr, "a=candidate:%s %hu %s %u %s %hu typ %s raddr %s rport %hu\n", c->foundation, c->component, ice_candidate_transport_name(c->protocol), c->priority, addr, addrport, ice_candidate_typename(c), raddr, raddrport);

	if (n < 0 || n >= s->end - s->ptr)
		return -1;
	s->ptr += n;
	return 0;
}

int ice_transport_getsdp(struct ice_transport_t* avt, int stream, char* buf, int bytes)
{
	int n;
	struct ice_transport_list_local_candidates_t p;
	p.stream = stream;
	p.ptr = buf;
	p.end = buf + bytes;

	if (avt->stun & ICE_LOCAL_ENABLE)
	{
		n = snprintf(p.ptr, bytes, "a=ice-ufrag:%s\na=ice-pwd:%s\n", avt->usr, avt->pwd);
		if (n < 0 || n >= bytes)
			return -1;
		p.ptr += n;

		if (0 != ice_agent_list_local_candidate(avt->ice, ice_transport_oncandidate, &p))
			return 0;
	}
	return (int)(p.ptr - buf);
}

struct ice_transport_gather_local_candidate_t
{
	struct ice_transport_t* avt;
	int foundation;
	int stream;
	int component;
};

static void ice_transport_gather_local_candidate_handler(void* param, const char* mac, const char* name, int dhcp, const char* ip, const char* netmask, const char* gateway)
{
	int i, j;
	u_short port[2];
	socklen_t addrlen;
	struct sockaddr_storage addr;
	struct ice_transport_gather_local_candidate_t* p;
	struct ice_transport_t* avt;

	(void)gateway, (void)netmask, (void)dhcp, (void)name, (void)mac;
	p = (struct ice_transport_gather_local_candidate_t*)param;
	avt = p->avt;

	if (NULL == ip || 0 == *ip || 0 == strcmp("0.0.0.0", ip))
		return; // ignore

	socket_addr_from(&addr, &addrlen, ip, 0);

	for (i = 0; i < p->stream; i++)
	{
		if (avt->naddr + p->component >= sizeof(avt->udp) / sizeof(avt->udp[0]))
			continue;

		if (2 == p->component)
			sockpair_create2((struct sockaddr*)&addr, &avt->udp[avt->naddr], port);
		else
			avt->udp[avt->naddr] = socket_udp_bind_addr((struct sockaddr*)&addr, 0, 0);

		for (j = 0; j < p->component && j < 2; j++)
		{
			getsockname(avt->udp[avt->naddr+j], (struct sockaddr*)&avt->addr[avt->naddr+j], &addrlen);
			ice_add_local_candidate(avt->ice, i, j+1, p->foundation + 1, (struct sockaddr*)&avt->addr[avt->naddr+j]);
		}

		avt->naddr += p->component;
	}

	++p->foundation;
}

int ice_transport_bind(struct ice_transport_t* avt, int stream, int component, const struct sockaddr* stun, int turn, const char* usr, const char* pwd)
{
	struct ice_transport_gather_local_candidate_t t;
	t.foundation = 0;
	t.component = component;
	t.stream = stream;
	t.avt = avt;

	assert(component <= 2);
	network_getip(ice_transport_gather_local_candidate_handler, &t);
	avt->stun = (avt->stun & ~ICE_LOCAL_ENABLE) | (stun ? ICE_LOCAL_ENABLE : 0);

	// start run thread
	assert(0 == avt->running);
	avt->running = 1;
	thread_create(&avt->thread, ice_transport_thread, avt);

	if (stun && (AF_INET == stun->sa_family || AF_INET6 == stun->sa_family))
	{
		return ice_agent_gather(avt->ice, (struct sockaddr*)stun, turn, 2000, STUN_CREDENTIAL_LONG_TERM, usr, pwd);
	}
	else
	{
		ice_transport_ongather(avt->param, 0);
		return 0;
	}
}

int ice_transport_getaddr(struct ice_transport_t* avt, int stream, int component, struct sockaddr_storage* local)
{
	struct ice_candidate_t c;

	memset(&c, 0, sizeof(c));
	if (0 != ice_agent_get_candidate(avt->ice, (uint8_t)stream, (uint16_t)component, &c))
		return -1;

	memcpy(local, &c.addr, sizeof(struct sockaddr_storage));
	return 0;
}

int ice_transport_connect(struct ice_transport_t* avt, const struct rtsp_media_t* avmedia, int count)
{
	int i, j, r;
	int64_t flags;
	socklen_t len;
	struct ice_candidate_t c;
	const struct rtsp_media_t *m;
	
	r = ice_add_remote_candidates(avt->ice, avmedia, count);
	avt->stun = (avt->stun & ~ICE_REMOTE_ENABLE) | (r > 0 ? ICE_REMOTE_ENABLE : 0);

	if (avt->stun & ICE_REMOTE_ENABLE)
		return ice_agent_start(avt->ice);

	// add default candidate(for non-ice mode)
	for (flags = i = 0; i < count; i++)
	{
		m = avmedia + i;
		for (j = 0; j < m->nport; j++)
		{
			memset(&c, 0, sizeof(c));
			c.stream = (uint8_t)i;
			c.priority = 1;
			c.component = j + 1;
			c.protocol = STUN_PROTOCOL_UDP;
			c.type = ICE_CANDIDATE_HOST;
			snprintf(c.foundation, sizeof(c.foundation), "%s", "default");
			socket_addr_from(&c.addr, &len, m->address, m->port[j]);
			memcpy(&c.host, &c.addr, sizeof(struct sockaddr_storage)); // remote candidate c.host = c.addr
			ice_agent_add_remote_candidate(avt->ice, &c);
		}

		flags |= (int64_t)1 << i;
	}
	
	avt->handler.onconnected(avt->param, flags);
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
	int i, r, n;
	struct ice_transport_t* avt;
	struct sockaddr_storage addr;
	socklen_t addrlen;

	avt = (struct ice_transport_t*)param;

	while (avt->running)
	{
		aio_timeout_process();

		r = socket_poll_read(avt->udp, sizeof(avt->udp) / sizeof(avt->udp[0]), 1000);
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
			for (i = 0; i < sizeof(avt->udp) / sizeof(avt->udp[0]); i++)
			{
				if (0 == (r & (1 << i)))
					continue;

				addrlen = sizeof(addr);
				n = socket_recvfrom(avt->udp[i], avt->ptr, sizeof(avt->ptr), 0, (struct sockaddr*)&addr, &addrlen);
				if (n <= 0)
					continue;

				n = ice_agent_input(avt->ice, STUN_PROTOCOL_UDP, (struct sockaddr*)&avt->addr[i], (struct sockaddr*)&addr, avt->ptr, n);
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
