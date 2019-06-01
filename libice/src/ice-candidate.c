#include "ice-candidate.h"
#include "ice-internal.h"
#include "md5.h"
#include <assert.h>

// RFC5245 4.1.1.3. Computing Foundations (p22)
void ice_candidate_foundation(struct ice_candidate_t* c)
{
	// An arbitrary string that is the same for two candidates that have the same type, 
	// base IP address, protocol (UDP, TCP, etc.), and STUN or TURN server.
	// 1. they are of the same type (host, relayed, server reflexive, or peer reflexive).
	// 2. their bases have the same IP address (the ports can be different).
	// 3. for reflexive and relayed candidates, the STUN or TURN servers used to obtain them have the same IP address.
	// 4. they were obtained using the same transport protocol (TCP, UDP, etc.).

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
	if (ICE_CANDIDATE_HOST != c->type && 0 != c->stun.ss_family)
		MD5Update(&ctx, (unsigned char*)&c->stun, socket_addr_len((struct sockaddr*)&c->stun));
	else
		MD5Update(&ctx, (unsigned char*)&c->host, socket_addr_len((struct sockaddr*)&c->host));
	MD5Final(md5, &ctx);

	assert(sizeof(c->foundation) >= 2 * sizeof(md5));
	for (i = 0; i < sizeof(md5) && i < sizeof(c->foundation); i++)
	{
		c->foundation[i * 2] = s_base16_enc[(md5[i] >> 4) & 0x0F];
		c->foundation[i * 2 + 1] = s_base16_enc[md5[i] & 0x0F];
	}
}
