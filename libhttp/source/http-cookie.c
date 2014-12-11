#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "http-cookie.h"

#if defined(OS_WINDOWS)
	#define strcasecmp _stricmp
#endif

struct http_cookie_t
{
	char* cookie;
	const char *name;
	const char *value;
	const char *path;
	const char *domain;
	const char *expires;
	int httponly;
	int secure;
};

cookie_t http_cookie_parse(const char* cookie)
{
	char *p;
	size_t n;
	struct http_cookie_t *ck;

	if(!cookie || 0==cookie[0])
		return NULL;

	n = strlen(cookie);
	ck = (struct http_cookie_t *)malloc(sizeof(*ck) + n + 1);
	if(!ck)
		return NULL;

	memset(ck, 0, sizeof(*ck));
	ck->cookie = (char*)(ck + 1);
	memcpy(ck->cookie, cookie, n+1);

	p = ck->cookie;
	while(p)
	{
		char *pn, *pv;

		while(' ' == *p || '\t' == *p) ++p;

		pn = strchr(p, ';');
		pv = strchr(p, '=');

		if(pn)
		{
			*pn = ' '; // replace ';' -> ' '
			while(' '==*pn || '\t'==*pn) --pn;
			++pn;
			*pn++ = '\0';
			while(' '==*pn || '\t'==*pn) ++pn;
		}

		if(pv && (!pn || pv < pn) )
		{
			*pv = ' '; // replace '=' -> ' '
			while(' ' == *pv || '\t' == *pv) --pv;
			++pv;
			*pv++ = '\0';
			while(' ' == *pv || '\t' == *pv) ++pv;

			if(!ck->name)
			{
				ck->name = p;
				ck->value = pv;
			}
			else if(0 == strcasecmp("path", p))
			{
				ck->path = pv;
			}
			else if(0 == strcasecmp("domain", p))
			{
				ck->domain = pv;
			}
			else if(0 == strcasecmp("expires", p))
			{
				ck->expires = pv;
			}
			else
			{
				// unknown attribute, ignore it
			}
		}
		else
		{
			if(0 == strcasecmp("HttpOnly", p))
			{
				ck->httponly = 1;
			}
			else if(0 == strcasecmp("Secure", p))
			{
				ck->secure = 1;
			}
			else
			{
				// unknown attribute, ignore it
			}
		}

		p = pn;
	}

	return ck;
}

void http_cookie_destroy(cookie_t cookie)
{
	struct http_cookie_t *ck;
	ck = (struct http_cookie_t *)cookie;
#if defined(DEBUG) || defined(_DEBUG)
	memset(ck, 0xCC, sizeof(*ck));
#endif
	free(ck);
}

const char* http_cookie_get_name(cookie_t cookie)
{
	struct http_cookie_t *ck;
	ck = (struct http_cookie_t *)cookie;
	return ck->name;
}

const char* http_cookie_get_value(cookie_t cookie)
{
	struct http_cookie_t *ck;
	ck = (struct http_cookie_t *)cookie;
	return ck->value;
}

const char* http_cookie_get_path(cookie_t cookie)
{
	struct http_cookie_t *ck;
	ck = (struct http_cookie_t *)cookie;
	return ck->path;
}

const char* http_cookie_get_domain(cookie_t cookie)
{
	struct http_cookie_t *ck;
	ck = (struct http_cookie_t *)cookie;
	return ck->domain;
}

const char* http_cookie_get_expires(cookie_t cookie)
{
	struct http_cookie_t *ck;
	ck = (struct http_cookie_t *)cookie;
	return ck->expires;
}

int http_cookie_is_httponly(cookie_t cookie)
{
	struct http_cookie_t *ck;
	ck = (struct http_cookie_t *)cookie;
	return ck->httponly;
}

int http_cookie_is_secure(cookie_t cookie)
{
	struct http_cookie_t *ck;
	ck = (struct http_cookie_t *)cookie;
	return ck->secure;
}

int http_cookie_check_path(cookie_t cookie, const char* path)
{
	size_t n;
	struct http_cookie_t *ck;
	ck = (struct http_cookie_t *)cookie;

	if(!ck->path) return 1;

	n = strlen(ck->path);
	assert(n > 0 && '/' == ck->path[n-1]);
	return (path && path[0] && 0==strncmp(path, ck->path, n-1) && ('/'==path[n-1] || '\0'==path[n-1])) ? 1 : 0;
}

int http_cookie_check_domain(cookie_t cookie, const char* domain)
{
	size_t n1, n2;
	struct http_cookie_t *ck;
	ck = (struct http_cookie_t *)cookie;

	if(!ck->domain) return 1;

	n1 = strlen(domain);
	n2 = strlen(ck->domain);
	assert(n1 > 0 && n2 > 0);
	return (n1 >= n2 && 0==strncmp(domain+(n1-n2), ck->domain, n2) && (n1==n2 || '.'==ck->domain[0] || '.'==domain[n1-n2-1])) ? 1 : 0;
}

int http_cookie_create(char cookie[], size_t bytes, const char* name, const char* value, const char* path, const char* domain, const char* expires, int httponly, int secure)
{
	size_t i, n1, n2;
	const char *names[] = { "path=", "domain=", "expires=", "HttpOnly", "Secure" };
	const char *attrs[5];

	n1 = strlen(name);
	n2 = strlen(value);
	if(n1 < 1 || n2 < 1)
		return -1;

	if(n1 + n2 + 2 > bytes)
		return -1;

	memcpy(cookie, name, n1);
	cookie[n1] = '=';
	memcpy(cookie+n1+1, value, n2);
	cookie += n1 + n2 + 1;
	bytes -= n1 + n2 + 1;
	cookie[0] = '\0';

	attrs[0] = path;
	attrs[1] = domain;
	attrs[2] = expires;
	attrs[3] = 1==httponly ? "" : NULL;
	attrs[4] = 1==secure ? "" : NULL;
	for(i = 0; i < sizeof(attrs)/sizeof(attrs[0]); i++)
	{
		if(!attrs[i]) continue;

		n1 = 2 + strlen(names[i]) + strlen(attrs[i]);
		if(n1 > bytes)
			return -1;

		sprintf(cookie, "; %s%s", names[i], attrs[i]);
		cookie += n1;
		bytes -= n1;
	}

	return 0;
}

#if defined(DEBUG) || defined(_DEBUG)
static void http_cookie_parse_test(void)
{
	cookie_t cookie;

	cookie = http_cookie_parse("LSID=DQAAAK¡­Eaem_vYg; Path=/accounts; Expires=Wed, 13 Jan 2021 22:23:01 GMT; Secure; HttpOnly");
	assert(0==strcmp("LSID", http_cookie_get_name(cookie)) && 0==strcmp("DQAAAK¡­Eaem_vYg", http_cookie_get_value(cookie)));
	assert(0==strcmp("Wed, 13 Jan 2021 22:23:01 GMT", http_cookie_get_expires(cookie)) && 0==strcmp("/accounts", http_cookie_get_path(cookie)) && !http_cookie_get_domain(cookie) && 1==http_cookie_is_httponly(cookie) && 1==http_cookie_is_secure(cookie));
	http_cookie_destroy(cookie);

	cookie = http_cookie_parse("HSID=AYQEVn¡­.DKrdst; Domain=.foo.com; Path=/; Expires=Wed, 13 Jan 2021 22:23:01 GMT; HttpOnly");
	assert(0==strcmp("HSID", http_cookie_get_name(cookie)) && 0==strcmp("AYQEVn¡­.DKrdst", http_cookie_get_value(cookie)));
	assert(0==strcmp("Wed, 13 Jan 2021 22:23:01 GMT", http_cookie_get_expires(cookie)) && 0==strcmp("/", http_cookie_get_path(cookie)) && 0==strcmp(".foo.com", http_cookie_get_domain(cookie)) && 1==http_cookie_is_httponly(cookie) && 0==http_cookie_is_secure(cookie));
	http_cookie_destroy(cookie);

	cookie = http_cookie_parse("SSID=Ap4P¡­.GTEq; Domain=foo.com; Path=/; Expires=Wed, 13 Jan 2021 22:23:01 GMT; Secure; HttpOnly");
	assert(0==strcmp("SSID", http_cookie_get_name(cookie)) && 0==strcmp("Ap4P¡­.GTEq", http_cookie_get_value(cookie)));
	assert(0==strcmp("Wed, 13 Jan 2021 22:23:01 GMT", http_cookie_get_expires(cookie)) && 0==strcmp("/", http_cookie_get_path(cookie)) && 0==strcmp("foo.com", http_cookie_get_domain(cookie)) && 1==http_cookie_is_httponly(cookie) && 1==http_cookie_is_secure(cookie));
	http_cookie_destroy(cookie);

	cookie = http_cookie_parse("made_write_conn=1295214458; Path=/; Domain=.example.com");
	assert(0==strcmp("made_write_conn", http_cookie_get_name(cookie)) && 0==strcmp("1295214458", http_cookie_get_value(cookie)));
	assert(!http_cookie_get_expires(cookie) && 0==strcmp("/", http_cookie_get_path(cookie)) && 0==strcmp(".example.com", http_cookie_get_domain(cookie)) && 0==http_cookie_is_httponly(cookie) && 0==http_cookie_is_secure(cookie));
	http_cookie_destroy(cookie);

	cookie = http_cookie_parse("name=value");
	assert(0==strcmp("name", http_cookie_get_name(cookie)) && 0==strcmp("value", http_cookie_get_value(cookie)));
	assert(!http_cookie_get_expires(cookie) && !http_cookie_get_path(cookie) && !http_cookie_get_domain(cookie) && 0==http_cookie_is_httponly(cookie) && 0==http_cookie_is_secure(cookie));
	http_cookie_destroy(cookie);

	cookie = http_cookie_parse("name2=value2; Expires=Wed, 09 Jun 2021 10:18:14 GMT");
	assert(0==strcmp("name2", http_cookie_get_name(cookie)) && 0==strcmp("value2", http_cookie_get_value(cookie)));
	assert(0==strcmp("Wed, 09 Jun 2021 10:18:14 GMT", http_cookie_get_expires(cookie)) && !http_cookie_get_path(cookie) && !http_cookie_get_domain(cookie) && 0==http_cookie_is_httponly(cookie) && 0==http_cookie_is_secure(cookie));
	http_cookie_destroy(cookie);
}

static void http_cookie_create_test(void)
{
	char cookie[128] = {0};
	assert(0==http_cookie_create(cookie, sizeof(cookie), "name", "value", NULL, NULL, NULL, 0, 0) && 0==strcmp(cookie, "name=value"));
	assert(0==http_cookie_create(cookie, sizeof(cookie), "name", "value", "/", NULL, NULL, 0, 0) && 0==strcmp(cookie, "name=value; path=/"));
	assert(0==http_cookie_create(cookie, sizeof(cookie), "name", "value", "/", "abc.com", NULL, 0, 0) && 0==strcmp(cookie, "name=value; path=/; domain=abc.com"));
	assert(0==http_cookie_create(cookie, sizeof(cookie), "name", "value", "/", "abc.com", "Wed, 09 Jun 2021 10:18:14 GMT", 0, 0) && 0==strcmp(cookie, "name=value; path=/; domain=abc.com; expires=Wed, 09 Jun 2021 10:18:14 GMT"));
	assert(0==http_cookie_create(cookie, sizeof(cookie), "name", "value", "/", "abc.com", "Wed, 09 Jun 2021 10:18:14 GMT", 1, 0) && 0==strcmp(cookie, "name=value; path=/; domain=abc.com; expires=Wed, 09 Jun 2021 10:18:14 GMT; HttpOnly"));
	assert(0==http_cookie_create(cookie, sizeof(cookie), "name", "value", "/", "abc.com", "Wed, 09 Jun 2021 10:18:14 GMT", 1, 1) && 0==strcmp(cookie, "name=value; path=/; domain=abc.com; expires=Wed, 09 Jun 2021 10:18:14 GMT; HttpOnly; Secure"));
	assert(0==http_cookie_create(cookie, sizeof(cookie), "name", "value", "/", NULL, NULL, 1, 1) && 0==strcmp(cookie, "name=value; path=/; HttpOnly; Secure"));
	assert(0==http_cookie_create(cookie, sizeof(cookie), "name", "value", NULL, NULL, NULL, 1, 0) && 0==strcmp(cookie, "name=value; HttpOnly"));
	assert(0==http_cookie_create(cookie, sizeof(cookie), "name", "value", "/", NULL, NULL, 0, 1) && 0==strcmp(cookie, "name=value; path=/; Secure"));
}

void http_cookie_test(void)
{
	http_cookie_parse_test();
	http_cookie_create_test();
}
#endif
