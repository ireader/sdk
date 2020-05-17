#include "http-header-auth.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

#if defined(OS_WINDOWS)
	#define strncasecmp	_strnicmp
#endif

// RFC2616 HTTP/1.1 2.2 Basic Rules (p13)
// CTL = 0-31 and DEL(127)
static const char* s_separators = "()<>@,;:\\\"/[]?={} \t";

static inline const char* strskip(const char* s)
{
	assert(s);
	while (' ' == *s || '\t' == *s) ++s;
	return s;
}

//static inline int istoken(char c)
//{
//	//token = 1*<any CHAR except CTLs or separators>
//	//1. CHAR = <any US-ASCII character (octets 0 - 127)>
//	//2. CTLs 0-31 and DEL(127)
//	//3. separators
//	if (c <= 31 || c >= 127 || strchr(s_separators, c))
//		return 0;
//	return 1;
//}

//static inline const char* strtoken(const char* s)
//{
//	assert(s);
//	while (*s && istoken(*s)) s++;
//	return s;
//}

static int http_header_authorization_scheme(const char* scheme, size_t bytes)
{
	if (0 == strncasecmp(scheme, "Basic", bytes))
	{
		return HTTP_AUTHENTICATION_BASIC;
	}
	else if (0 == strncasecmp(scheme, "Digest", bytes))
	{
		return HTTP_AUTHENTICATION_DIGEST;
	}
	else
	{
		assert(0);
		return HTTP_AUTHENTICATION_NONE; // unknown
	}
}

static int http_header_authorization_param(struct http_header_www_authenticate_t* auth, const char* name, size_t bytes, const char* value, size_t bytes2)
{
	int r;
	if (8 == bytes && 0 == strncasecmp(name, "username", bytes))
	{
		r = snprintf(auth->username, sizeof(auth->username), "%.*s", (int)bytes2, value);
		return r < 0 || r >= sizeof(auth->username) ? (r < 0 ? r : -E2BIG) : 0;
	}
	else if (5 == bytes && 0 == strncasecmp(name, "realm", bytes))
	{
		r = snprintf(auth->realm, sizeof(auth->realm), "%.*s", (int)bytes2, value);
		return r < 0 || r >= sizeof(auth->realm) ? (r < 0 ? r : -E2BIG) : 0;
	}
	else if (5 == bytes && 0 == strncasecmp(name, "nonce", bytes))
	{
		r = snprintf(auth->nonce, sizeof(auth->nonce), "%.*s", (int)bytes2, value);
		return r < 0 || r >= sizeof(auth->nonce) ? (r < 0 ? r : -E2BIG) : 0;
	}
	else if (3 == bytes && 0 == strncasecmp(name, "uri", bytes))
	{
		r = snprintf(auth->uri, sizeof(auth->uri), "%.*s", (int)bytes2, value);
		return r < 0 || r >= sizeof(auth->uri) ? (r < 0 ? r : -E2BIG) : 0;
	}
	else if (8 == bytes && 0 == strncasecmp(name, "response", bytes))
	{
		r = snprintf(auth->response, sizeof(auth->response), "%.*s", (int)bytes2, value);
		return r < 0 || r >= sizeof(auth->response) ? (r < 0 ? r : -E2BIG) : 0;
	}
	else if (9 == bytes && 0 == strncasecmp(name, "algorithm", bytes))
	{
		r = snprintf(auth->algorithm, sizeof(auth->algorithm), "%.*s", (int)bytes2, value);
		return r < 0 || r >= sizeof(auth->algorithm) ? (r < 0 ? r : -E2BIG) : 0;
	}
	else if (6 == bytes && 0 == strncasecmp(name, "cnonce", bytes))
	{
		r = snprintf(auth->cnonce, sizeof(auth->cnonce), "%.*s", (int)bytes2, value);
		return r < 0 || r >= sizeof(auth->cnonce) ? (r < 0 ? r : -E2BIG) : 0;
	}
	else if (6 == bytes && 0 == strncasecmp(name, "opaque", bytes))
	{
		r = snprintf(auth->opaque, sizeof(auth->opaque), "%.*s", (int)bytes2, value);
		return r < 0 || r >= sizeof(auth->opaque) ? (r < 0 ? r : -E2BIG) : 0;
	}
	else if (3 == bytes && 0 == strncasecmp(name, "qop", bytes))
	{
		r = snprintf(auth->qop, sizeof(auth->qop), "%.*s", (int)bytes2, value);
		return r < 0 || r >= sizeof(auth->qop) ? (r < 0 ? r : -E2BIG) : 0;
	}
	else if (2 == bytes && 0 == strncasecmp(name, "nc", bytes))
	{
		// 8LHEX, The nc value is the hexadecimal count of the number of requests
		//return s_strcpy(auth->nc, sizeof(auth->nc), value, bytes2);
		r = sscanf(value, "%x", &auth->nc);
		return 1 != r ? -EINVAL : 0;
	}
	else
	{
		// ignore
	}

	return 0;
}

int http_header_authorization(const char* field, struct http_header_www_authenticate_t* auth)
{
	const char* name;
	const char* value;
	size_t bytes, bytes2;

	// auth-scheme
	field = strskip(field);
	bytes = strcspn(field, " \t\r\n"); // get scheme length
	auth->scheme = http_header_authorization_scheme(field, bytes);

	if (HTTP_AUTHENTICATION_BASIC == auth->scheme)
	{
		snprintf(auth->algorithm, sizeof(auth->algorithm), "base64");

		// RFC2617 2. Basic Authentication Scheme
		// credentials = "Basic" basic-credentials
		value = strskip(field + bytes);
		bytes = strcspn(value, " \t\r\n");
		snprintf(auth->response, sizeof(auth->response), "%.*s", (int)bytes, value);
	}
	else
	{
		assert(HTTP_AUTHENTICATION_DIGEST == auth->scheme);
		// auth-param
		for(field += bytes; *field; field = value + bytes2)
		{
			name = field + strspn(field, ", \t");
			bytes = strcspn(name, " \t=\r\n");

			field = strskip(name + bytes);
			if ('=' != *field)
				break;

			value = strskip(field + 1);
			if ('"' == *value)
			{
				value += 1; // skip \"
				bytes2 = strcspn(value, "\"");
				http_header_authorization_param(auth, name, bytes, value, bytes2);

				assert('"' == value[bytes2]);
				if ('"' == value[bytes2])
					bytes2 += 1; // skip \"
			}
			else
			{
				bytes2 = strcspn(value, ", \t\r\n");
				http_header_authorization_param(auth, name, bytes, value, bytes2);
			}
		}
	}

	return 0;
}

int http_header_authorization_write(const struct http_header_www_authenticate_t* auth, char* ptr, int len)
{
	int n;
	switch (auth->scheme)
	{
	case HTTP_AUTHENTICATION_DIGEST:
		n = snprintf(ptr, len, "Digest realm=\"%s\",nonce=\"%s\",username=\"%s\",uri=\"%s\",response=\"%s\"",
			auth->realm, auth->nonce, /*auth->userhash ? username : */ auth->username, auth->uri, auth->response);
		if (0 != auth->algorithm[0])
			n += snprintf(ptr + n, len - n, ",algorithm=%s", auth->algorithm);
		if (0 != auth->opaque[0])
			n += snprintf(ptr + n, len - n, ",opaque=\"%s\"", auth->opaque);
		if (0 != auth->qop[0])
			n += snprintf(ptr + n, len - n, ",cnonce=\"%s\",nc=%08x,qop=%s", auth->cnonce, auth->nc, auth->qop);
		return n;

	case HTTP_AUTHENTICATION_BASIC:
		// base64(usr:pwd)
		// base64("Aladdin:open sesame") => QWxhZGRpbjpvcGVuIHNlc2FtZQ==
		n = snprintf(ptr, len, "Basic %s", auth->response);
		return -1;

	default:
		assert(0);
		return -1;
	}
}

#if defined(_DEBUG) || defined(DEBUG)
void http_header_authorization_test(void)
{
	struct http_header_www_authenticate_t authorization;

	http_header_authorization("Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==", &authorization);
	assert(HTTP_AUTHENTICATION_BASIC == authorization.scheme);
	assert(0 == strcmp("QWxhZGRpbjpvcGVuIHNlc2FtZQ==", authorization.response));

	/*
	Authorization: Digest username="Mufasa",
					realm="http-auth@example.org",
					uri="/dir/index.html",
					algorithm=MD5,
					nonce="7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v",
					nc=00000001,
					cnonce="f2/wE4q74E6zIJEtWaHKaf5wv/H5QzzpXusqGemxURZJ",
					qop=auth,
					response="8ca523f5e9506fed4657c9700eebdbec",
					opaque="FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS"
	*/
	http_header_authorization("Digest username=\"Mufasa\", \
		realm = \"http-auth@example.org\", \
		uri = \"/dir/index.html\", \
		algorithm = MD5, \
		nonce = \"7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v\", \
		nc = 0000000f, \
		cnonce = \"f2/wE4q74E6zIJEtWaHKaf5wv/H5QzzpXusqGemxURZJ\", \
		qop = auth, \
		response = \"8ca523f5e9506fed4657c9700eebdbec\", \
		opaque = \"FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS\"", 
		&authorization);

	assert(HTTP_AUTHENTICATION_DIGEST == authorization.scheme);
	assert(0 == strcmp("Mufasa", authorization.username));
	assert(0 == strcmp("http-auth@example.org", authorization.realm));
	assert(0 == strcmp("/dir/index.html", authorization.uri));
	assert(0 == strcmp("MD5", authorization.algorithm));
	assert(0 == strcmp("7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v", authorization.nonce));
	assert(0xf == authorization.nc);
	assert(0 == strcmp("f2/wE4q74E6zIJEtWaHKaf5wv/H5QzzpXusqGemxURZJ", authorization.cnonce));
	assert(0 == strcmp("auth", authorization.qop));
	assert(0 == strcmp("8ca523f5e9506fed4657c9700eebdbec", authorization.response));
	assert(0 == strcmp("FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS", authorization.opaque));
}
#endif
