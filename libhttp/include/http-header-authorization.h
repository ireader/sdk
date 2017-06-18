#ifndef _http_header_authorization_h_
#define _http_header_authorization_h_

struct http_header_authorization_t
{
	int scheme; // 1-basic, 2-digest
	char username[128];
	char realm[128]; // case-sensitive
	char nonce[128];
	char uri[256];
	char response[256];
	char algorithm[64];
	char cnonce[128];
	char opaque[256];
	char qop[64];
	int nc; // nonce count
	int userhash; // 0-false(default), 1-true
};

// For historical reasons, a sender MUST only generate the quoted string
// syntax for the following parameters : username, realm, nonce, uri,
// response, cnonce, and opaque
// For historical reasons, a sender MUST NOT generate the quoted string
// syntax for the following parameters : algorithm, qop, and nc.

// WWW-Authenticate: Basic realm="WallyWorld"
// Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ== (userid "Aladdin" and password "open sesame")
int http_header_authorization(const char* field, struct http_header_authorization_t* authorization);

#endif /* !_http_header_authorization_h_ */
