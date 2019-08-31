#include "ice-internal.h"
#include "ice-checklist.h"
#include "ice-candidates.h"
#include "stun-internal.h"
#include <assert.h>

// rfc5245 7.1.3.2.1. Discovering Peer Reflexive Candidates (p43)
int ice_agent_add_peer_reflexive_candidate(struct ice_agent_t* ice, const struct stun_address_t* addr, const struct stun_attr_t* priority)
{
	struct ice_candidate_t c, *local;
	if (0 == addr->reflexive.ss_family)
		return -1;

	memset(&c, 0, sizeof(struct ice_candidate_t));
	c.protocol = addr->protocol;
	memcpy(&c.host, &addr->host, sizeof(c.host));
	memcpy(&c.addr, &addr->reflexive, sizeof(c.addr));
	//assert(0 == addr->relay.ss_family);

	local = ice_agent_find_local_candidate(ice, &c.host);
	if (NULL == local)
		return -1; // local not found, new request ???

	c.type = ICE_CANDIDATE_PEER_REFLEXIVE;
	c.stream = local->stream;
	c.component = local->component;
	ice_candidate_foundation(ice, &c);
	ice_candidate_priority(&c);
	if (priority)
		c.priority = priority->v.u32;
	return ice_agent_add_local_candidate(ice, &c);
}

// rfc5245 7.2.1.3. Learning Peer Reflexive Candidates (p49)
int ice_agent_add_remote_peer_reflexive_candidate(struct ice_agent_t* ice, uint8_t stream, uint16_t component, const struct stun_address_t* addr, const struct stun_attr_t* priority)
{
	struct ice_candidate_t c;

	assert(0 == addr->reflexive.ss_family);
	memset(&c, 0, sizeof(struct ice_candidate_t));
	c.protocol = addr->protocol;
	memcpy(&c.host, &addr->peer, sizeof(c.host));
	memcpy(&c.addr, &addr->peer, sizeof(c.addr));
	//memcpy(&c.stun, &addr->host, sizeof(c.stun));
	//assert(0 == addr->relay.ss_family);

	c.type = ICE_CANDIDATE_PEER_REFLEXIVE;
	c.stream = stream;
	c.component = component;
	// The foundation of the candidate is set to an arbitrary value,
	// different from the foundation for all other remote candidates.
	c.foundation[0] = '\0'; // ice_candidate_foundation(&c);
	ice_candidate_priority(&c);
	if (priority)
		c.priority = priority->v.u32;
	return ice_agent_add_remote_candidate(ice, &c);
}

int ice_agent_bind(struct ice_agent_t* ice, const struct stun_credential_t* auth, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, int timeout, stun_request_handler handler, void* param)
{
	struct stun_request_t* req;
	req = stun_request_create(ice->stun, STUN_RFC_5389, handler, param);
	if (!req) return -1;

	// rfc5245 4.1.1.2. Server Reflexive and Relayed Candidates (p20)
	// Binding requests to a STUN server are not authenticated, and 
	// any ALTERNATESERVER attribute in a response is ignored.
	stun_request_settimeout(req, timeout);
	stun_request_setaddr(req, STUN_PROTOCOL_UDP, local, remote, relay);
	stun_request_setauth(req, auth->credential, auth->usr, auth->pwd, auth->realm, auth->nonce);
	return stun_agent_bind(req);
}

int ice_agent_allocate(struct ice_agent_t* ice, const struct stun_credential_t* auth, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, int timeout, stun_request_handler handler, void* param)
{
	struct stun_request_t* req;
	req = stun_request_create(ice->stun, STUN_RFC_5389, handler, param);
	if (!req) return -1;

	// rfc5245 4.1.1.2. Server Reflexive and Relayed Candidates (p20)
	// Allocate requests SHOULD be authenticated using a longterm
	// credential obtained by the client through some other means.
	stun_request_settimeout(req, timeout);
	stun_request_setaddr(req, STUN_PROTOCOL_UDP, local, remote, relay);
	stun_request_setauth(req, auth->credential, auth->usr, auth->pwd, auth->realm, auth->nonce);
	return turn_agent_allocate(req, TURN_TRANSPORT_UDP);
}

int ice_agent_refresh(struct ice_agent_t* ice, const struct stun_credential_t* auth, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, int timeout, stun_request_handler handler, void* param)
{
	struct stun_request_t* req;
	req = stun_request_create(ice->stun, STUN_RFC_5389, handler, param);
	if (!req) return -1;

	// rfc5245 4.1.1.2. Server Reflexive and Relayed Candidates (p20)
	// Allocate requests SHOULD be authenticated using a longterm
	// credential obtained by the client through some other means.
	stun_request_settimeout(req, timeout);
	stun_request_setaddr(req, STUN_PROTOCOL_UDP, local, remote, relay);
	stun_request_setauth(req, auth->credential, auth->usr, auth->pwd, auth->realm, auth->nonce);
	return turn_agent_refresh(req, TURN_LIFETIME);
}

int ice_agent_create_permission(struct ice_agent_t* ice, const struct stun_credential_t* auth, const struct sockaddr* local, const struct sockaddr* turn, const struct sockaddr* peer, int timeout, stun_request_handler handler, void* param)
{
	struct stun_request_t* req;
	req = stun_request_create(ice->stun, STUN_RFC_5389, handler, param);
	if (!req) return -1;

	// rfc5245 4.1.1.2. Server Reflexive and Relayed Candidates (p20)
	// Allocate requests SHOULD be authenticated using a longterm
	// credential obtained by the client through some other means.
	stun_request_settimeout(req, timeout);
	stun_request_setaddr(req, STUN_PROTOCOL_UDP, local, turn, NULL);
	stun_request_setauth(req, auth->credential, auth->usr, auth->pwd, auth->realm, auth->nonce);
	return turn_agent_create_permission(req, peer);
}

int ice_agent_channel_bind(struct ice_agent_t* ice, const struct stun_credential_t* auth, const struct sockaddr* local, const struct sockaddr* turn, const struct sockaddr* peer, uint16_t channel, int timeout, stun_request_handler handler, void* param)
{
	struct stun_request_t* req;
	req = stun_request_create(ice->stun, STUN_RFC_5389, handler, param);
	if (!req) return -1;

	// rfc5245 4.1.1.2. Server Reflexive and Relayed Candidates (p20)
	// Allocate requests SHOULD be authenticated using a longterm
	// credential obtained by the client through some other means.
	stun_request_settimeout(req, timeout);
	stun_request_setaddr(req, STUN_PROTOCOL_UDP, local, turn, NULL);
	stun_request_setauth(req, auth->credential, auth->usr, auth->pwd, auth->realm, auth->nonce);
	return turn_agent_channel_bind(req, peer, channel);
}

int ice_agent_connect(struct ice_agent_t* ice, const struct ice_candidate_pair_t* pr, int nominated, int timeout, stun_request_handler handler, void* param)
{
	char user[512];
	struct stun_request_t* req;
	req = stun_request_create(ice->stun, STUN_RFC_5389, handler, param);
	if (!req) return -1;

	stun_request_settimeout(req, timeout);
	stun_request_setaddr(req, STUN_PROTOCOL_UDP, (const struct sockaddr*)&pr->local.host, (const struct sockaddr*)&pr->remote.host, ICE_CANDIDATE_RELAYED == pr->local.type ? (const struct sockaddr*)&pr->local.addr : NULL);
	// rfc5245 7.1.2. Sending the Request (p40)
	// 1. A connectivity check MUST utilize the STUN short-term credential mechanism
	// 2. The FINGERPRINT mechanism MUST be used for connectivity checks.
	// * The username for the credential is formed by concatenating the username fragment provided by the peer with 
	//   the username fragment of the agent sending the request, separated by a colon (":"). 
	// * The password is equal to the password provided by the peer.
	// * L -> R: A connectivity check from L to R utilizes the username RFRAG:LFRAG and a password of RPASS.
	snprintf(user, sizeof(user), "%s:%s", pr->stream->rauth.usr, ice->auth.usr);
	stun_request_setauth(req, ice->auth.credential, user, pr->stream->rauth.pwd, ice->auth.realm, ice->auth.nonce);
	// 3. An agent MUST include the PRIORITY attribute in its Binding request
	stun_message_add_uint32(&req->msg, STUN_ATTR_PRIORITY, pr->local.priority);
	// 4. The controlling agent MAY include the USE-CANDIDATE attribute in the Binding request. The controlled agent MUST NOT include it in its Binding request.
	if (ice->controlling && (nominated || ice->nomination == ICE_AGGRESSIVE_NOMINATION))
		stun_message_add_flag(&req->msg, STUN_ATTR_USE_CANDIDATE);
	// 5. The agent MUST include the ICE-CONTROLLED attribute in the request if it is in the controlled role, and MUST include the ICE-CONTROLLING attribute in the request if it is in the controlling role.
	stun_message_add_uint64(&req->msg, ice->controlling ? STUN_ATTR_ICE_CONTROLLING : STUN_ATTR_ICE_CONTROLLED, ice->tiebreaking);
	return stun_agent_bind(req);
}
