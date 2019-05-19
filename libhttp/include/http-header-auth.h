#ifndef _http_header_auth_h_
#define _http_header_auth_h_

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	HTTP_AUTHENTICATION_NONE = 0,
	HTTP_AUTHENTICATION_BASIC,
	HTTP_AUTHENTICATION_DIGEST,
};

struct http_header_www_authenticate_t
{
	int scheme; // HTTP_AUTHENTICATION_BASIC, HTTP_AUTHENTICATION_DIGEST
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

	// Authorization/Proxy-Authorization only
	char cnonce[128];
	char username[128];
	char uri[256];
	char response[256];
	int nc; //char nc[9]; // 8LHEX nonce count
};

// For historical reasons, a sender MUST only generate the quoted string
// syntax values for the following parameters : realm, domain, nonce,
// opaque, and qop.
// For historical reasons, a sender MUST NOT generate the quoted string
// syntax values for the following parameters : stale and algorithm.
int http_header_www_authenticate(const char* field, struct http_header_www_authenticate_t* auth);


// For historical reasons, a sender MUST only generate the quoted string
// syntax for the following parameters : username, realm, nonce, uri,
// response, cnonce, and opaque
// For historical reasons, a sender MUST NOT generate the quoted string
// syntax for the following parameters : algorithm, qop, and nc.
// WWW-Authenticate: Basic realm="WallyWorld"
// Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ== (userid "Aladdin" and password "open sesame")
int http_header_authorization(const char* field, struct http_header_www_authenticate_t* authorization);

// Digest username="Mufasa",realm="http-auth@example.org",uri="/",algorithm=MD5,nonce="xwf94BcCAzFZH4GiTo0v",nc=00000001,cnonce="emxURZJ",qop=auth,response="8ca523f5e9506fed4657c9700eebdbec",opaque="FQhe"
int http_header_authorization_write(const struct http_header_www_authenticate_t* auth, char* ptr, int len);

// Authorization
int http_header_auth(const struct http_header_www_authenticate_t* auth, const char* pwd, const char* method, const char* content, int length, char* authenrization, int bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_http_header_auth_h_ */
