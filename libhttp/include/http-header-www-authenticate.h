#ifndef _http_header_www_authenticate_h_
#define _http_header_www_authenticate_h_

struct http_header_www_authenticate_t
{
	int scheme; // 1-basic, 2-digest
	char realm[128]; // case-sensitive
	char domain[128];
	char nonce[128];
	char opaque[128];
	int stale; // 0-false, 1-true
	char algorithm[64];
	char qop[64];
	int charset; // UTF-8
	int userhash; // username hashing, 0-false(default), 1-true
	int session_variant;
};

// For historical reasons, a sender MUST only generate the quoted string
// syntax values for the following parameters : realm, domain, nonce,
// opaque, and qop.
// For historical reasons, a sender MUST NOT generate the quoted string
// syntax values for the following parameters : stale and algorithm.

int http_header_www_authenticate(const char* field, struct http_header_www_authenticate_t* content);

#endif /* !_http_header_www_authenticate_h_ */
