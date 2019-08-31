#include "ice-internal.h"
#include "ice-candidate.h"
#include "ice-candidates.h"
#include "turn-internal.h"
#include "md5.h"
#include <assert.h>

// Deprecated
static inline void ice_candidate_foundation0(struct ice_candidate_t* c)
{
	int i;
	MD5_CTX ctx;
	unsigned char md5[16];
	const struct sockaddr_storage* base;
	static const char* s_base16_enc = "0123456789ABCDEF";

	base = ice_candidate_base(c);

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)&c->type, sizeof(c->type));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)&c->protocol, sizeof(c->protocol));
	MD5Update(&ctx, (unsigned char*)":", 1);

	MD5Update(&ctx, (unsigned char*)&base->ss_family, sizeof(base->ss_family));
	if (AF_INET == base->ss_family)
		MD5Update(&ctx, (unsigned char*)&((struct sockaddr_in*)base)->sin_addr, 4);
	else if (AF_INET6 == base->ss_family)
		MD5Update(&ctx, (unsigned char*)&((struct sockaddr_in6*)base)->sin6_addr, 8);

	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)&c->addr, socket_addr_len((struct sockaddr*)&c->addr));
	MD5Final(md5, &ctx);

	assert(sizeof(c->foundation) >= 2 * sizeof(md5));
	for (i = 0; i < sizeof(md5) && i < sizeof(c->foundation); i++)
	{
		c->foundation[i * 2] = s_base16_enc[(md5[i] >> 4) & 0x0F];
		c->foundation[i * 2 + 1] = s_base16_enc[md5[i] & 0x0F];
	}
}

// RFC5245 4.1.1.3. Computing Foundations (p22)
void ice_candidate_foundation(struct ice_agent_t* ice, struct ice_candidate_t* c)
{
	// An arbitrary string that is the same for two candidates that have the same type, 
	// base IP address, protocol (UDP, TCP, etc.), and STUN or TURN server.
	// 1. they are of the same type (host, relayed, server reflexive, or peer reflexive).
	// 2. their bases have the same IP address (the ports can be different).
	// 3. for reflexive and relayed candidates, the STUN or TURN servers used to obtain them have the same IP address.
	// 4. they were obtained using the same transport protocol (TCP, UDP, etc.).

	int i, j;
	struct list_head* ptr;
	struct ice_stream_t* s;
	struct ice_candidate_t* p;
	char foundation[sizeof(c->foundation)];

	j = 16;
	snprintf(foundation, sizeof(foundation), "%d", j);

	list_for_each(ptr, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		for (i = 0; i < ice_candidates_count(&s->locals); i++)
		{
			p = ice_candidates_get(&s->locals, i);
			if (p->type == c->type && p->protocol == c->protocol && 0 == turn_sockaddr_cmp((struct sockaddr*)&p->host, (struct sockaddr*)&c->host))
			{
				memcpy(c->foundation, p->foundation, sizeof(c->foundation));
				return;
			}
			else if (0 == strcmp(p->foundation, foundation))
			{
				// just try it
				snprintf(foundation, sizeof(foundation), "%d", ++j);
			}
		}
	}

FOUNDATION_LOOP:
	list_for_each(ptr, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		for (i = 0; i < ice_candidates_count(&s->locals); i++)
		{
			p = ice_candidates_get(&s->locals, i);
			if (0 == strcmp(p->foundation, foundation))
			{
				snprintf(foundation, sizeof(foundation), "%d", ++j);
				goto FOUNDATION_LOOP;
			}
		}
	}

	// finally get a foundation
	memcpy(c->foundation, foundation, sizeof(c->foundation));
}
