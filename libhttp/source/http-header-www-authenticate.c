#include "http-header-auth.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#if defined(OS_WINDOWS)
	#define strncasecmp	_strnicmp
#endif

static const char* strskip(const char* s)
{
	assert(s);
	while (' ' == *s || '\t' == *s) ++s;
	return s;
}

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

static int s_strcpy(char* dst, size_t size, const char* src, size_t bytes)
{
	if (bytes + 1 > size)
		return -E2BIG;

	memcpy(dst, src, bytes);
	dst[bytes] = 0;
	return 0;
}

static int http_header_authorization_param(struct http_header_www_authenticate_t* auth, const char* name, size_t bytes, const char* value, size_t bytes2)
{
	if (0 == strncasecmp(name, "realm", bytes))
	{
		return s_strcpy(auth->realm, sizeof(auth->realm), value, bytes2);
	}
	else if (0 == strncasecmp(name, "domain", bytes))
	{
		return s_strcpy(auth->domain, sizeof(auth->domain), value, bytes2);
	}
	else if (0 == strncasecmp(name, "nonce", bytes))
	{
		return s_strcpy(auth->nonce, sizeof(auth->nonce), value, bytes2);
	}
	else if (0 == strncasecmp(name, "opaque", bytes))
	{
		return s_strcpy(auth->opaque, sizeof(auth->opaque), value, bytes2);
	}
	else if (0 == strncasecmp(name, "stale", bytes))
	{
		if (0 == strncasecmp(value, "true", bytes2))
		{
			auth->stale = 1;
		}
		else if (0 == strncasecmp(value, "false", bytes2))
		{
			auth->stale = 0;
		}
		else
		{
			assert(0);
			auth->stale = -1; // invalid value;
		}
	}
	else if (0 == strncasecmp(name, "algorithm", bytes))
	{
		return s_strcpy(auth->algorithm, sizeof(auth->algorithm), value, bytes2);
	}
	else if (0 == strncasecmp(name, "qop", bytes))
	{
		return s_strcpy(auth->qop, sizeof(auth->qop), value, bytes2);
	}
	else
	{
		// ignore
	}

	return 0;
}

int http_header_www_authenticate(const char* field, struct http_header_www_authenticate_t* auth)
{
	const char* name;
	const char* value;
	size_t bytes, bytes2;

	// auth-scheme
	field = strskip(field);
	bytes = strcspn(field, " \t\r\n"); // get scheme length
	auth->scheme = http_header_authorization_scheme(field, bytes);

	// auth-param
	for (field += bytes; *field; field = value + bytes2)
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

	return 0;
}

#if defined(_DEBUG) || defined(DEBUG)
void http_header_www_authenticate_test(void)
{
	struct http_header_www_authenticate_t authorization;

	http_header_www_authenticate("Basic realm=\"WallyWorld\"", &authorization);
	assert(HTTP_AUTHENTICATION_BASIC == authorization.scheme);
	assert(0 == strcmp("WallyWorld", authorization.realm));

	/*
	WWW-Authenticate: Digest
					realm="http-auth@example.org",
					qop="auth, auth-int",
					algorithm=SHA-256,
					nonce="7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v",
					opaque="FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS"
	*/

	http_header_www_authenticate("Digest			\
		realm = \"http-auth@example.org\",		\
		qop = \"auth, auth-int\",				\
		algorithm = SHA-256,					\
		nonce = \"7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v\", \
		opaque = \"FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS\"",
		&authorization);

	assert(HTTP_AUTHENTICATION_DIGEST == authorization.scheme);
	assert(0 == strcmp("http-auth@example.org", authorization.realm));
	assert(0 == strcmp("SHA-256", authorization.algorithm));
	assert(0 == strcmp("7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v", authorization.nonce));
	assert(0 == strcmp("auth, auth-int", authorization.qop));
	assert(0 == strcmp("FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS", authorization.opaque));
}
#endif
