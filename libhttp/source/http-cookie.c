#include "http-cookie.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "cstringext.h"

#define isblank(c) (' '==(c) || '\t'==(c))

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

struct http_cookie_t* http_cookie_parse(const char* cookie, size_t bytes)
{
	char *p;
	struct http_cookie_t *ck;

	if(!cookie || 0 == cookie[0] || 0 == bytes)
		return NULL;

	ck = (struct http_cookie_t *)calloc(1, sizeof(*ck) + bytes + 1);
	if(!ck)
		return NULL;

	ck->cookie = (char*)(ck + 1);
	memcpy(ck->cookie, cookie, bytes);
	ck->cookie[bytes] = '\0';

	p = ck->cookie;
	while(p)
	{
		char *pn, *pv;

		while(isblank(*p)) ++p;

		pn = strchr(p, ';');
		pv = strchr(p, '=');

		if(pn)
		{
			*pn = ' '; // replace ';' -> ' '
			while(isblank(*pn)) --pn;
			++pn; *pn++ = '\0';
			while(isblank(*pn)) ++pn;
		}

		if(pv && (!pn || pv < pn) )
		{
			*pv = ' '; // replace '=' -> ' '
			while(isblank(*pv)) --pv;
			++pv; *pv++ = '\0';
			while(isblank(*pv)) ++pv;

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

void http_cookie_destroy(struct http_cookie_t* ck)
{
#if defined(DEBUG) || defined(_DEBUG)
	memset(ck, 0xCC, sizeof(*ck));
#endif
	free(ck);
}

const char* http_cookie_get_name(struct http_cookie_t* ck)
{
	return ck->name;
}

const char* http_cookie_get_value(struct http_cookie_t* ck)
{
	return ck->value;
}

const char* http_cookie_get_path(struct http_cookie_t* ck)
{
	return ck->path;
}

const char* http_cookie_get_domain(struct http_cookie_t* ck)
{
	return ck->domain;
}

const char* http_cookie_get_expires(struct http_cookie_t* ck)
{
	return ck->expires;
}

int http_cookie_is_httponly(struct http_cookie_t* ck)
{
	return ck->httponly;
}

int http_cookie_is_secure(struct http_cookie_t* ck)
{
	return ck->secure;
}

int http_cookie_check_path(struct http_cookie_t* ck, const char* path)
{
	size_t n;
	if(!ck->path || 0 == ck->path[0]) return 1; // empty path

	n = strlen(ck->path);
	assert(n > 0 && '/' == ck->path[n-1]);
	return (path && path[0] && 0==strncmp(path, ck->path, n-1) && ('/'==path[n-1] || '\0'==path[n-1])) ? 1 : 0;
}

int http_cookie_check_domain(struct http_cookie_t* ck, const char* domain)
{
	size_t n1, n2;

	if(!domain || 0 == domain[0]) return 0;
	if(!ck->domain || 0 == ck->domain[0]) return 1;

	n1 = strlen(domain);
	n2 = strlen(ck->domain);
	assert(n1 > 0 && n2 > 0);
	return (n1 >= n2 && 0==strncmp(domain+(n1-n2), ck->domain, n2) && (n1==n2 || '.'==ck->domain[0] || '.'==domain[n1-n2-1])) ? 1 : 0;
}

int http_cookie_make(char cookie[], size_t bytes, const char* name, const char* value, const char* path, const char* domain, const char* expires, int httponly, int secure)
{
	size_t i, n1, n2;
	const char *names[] = { "path=", "domain=", "expires=", "HttpOnly", "Secure" };
	const char *attrs[5];

	n1 = name ? strlen(name) : 0;
	n2 = value ? strlen(value) : 0;
	if(n1 < 1 || n2 < 1)
		return -1;

	if(n1 + n2 + 2 > bytes)
		return -1;

	memcpy(cookie, name, n1);
	cookie[n1] = '=';
	memcpy(cookie+n1+1, value, n2);
	cookie += n1 + n2 + 1;
	bytes -= n1 + n2 + 1;

	attrs[0] = path;
	attrs[1] = domain;
	attrs[2] = expires;
	attrs[3] = 1==httponly ? "" : NULL;
	attrs[4] = 1==secure ? "" : NULL;
	for(i = 0; i < sizeof(attrs)/sizeof(attrs[0]); i++)
	{
		if(!attrs[i]) continue;

		n1 = strlen(names[i]);
		n2 = strlen(attrs[i]);
		if(n1 + n2 + 3 > bytes)
			return -1;

		memcpy(cookie, "; ", 2);
		memcpy(cookie+2, names[i], n1);
		memcpy(cookie+n1+2, attrs[i], n2);
		cookie += n1 + n2 + 2;
		bytes -= n1 + n2 + 2;
	}

	cookie[0] = '\0';
	return 0;
}

int http_cookie_expires(char expires[30], int hours)
{
	int n;
	time_t t;
	struct tm* gmt;
	static const char week[][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char month[][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	t = time(NULL);
	t += hours * 3600; // current + expireDay
	gmt = gmtime(&t);

	n = snprintf(expires, 30, "%s, %02d-%s-%04d %02d:%02d:%02d GMT",
		week[((unsigned int)gmt->tm_wday) % 7],	// weekday
		gmt->tm_mday,		// day
		month[((unsigned int)gmt->tm_mon) % 12],// month
		gmt->tm_year+1900,	// year
		gmt->tm_hour,		// hour
		gmt->tm_min,		// minute
		gmt->tm_sec);		// second

	return n < 0 || n >= 30 ? -1 : 0;
}

#if defined(DEBUG) || defined(_DEBUG)
static void http_cookie_parse_test(void)
{
	const char* p;
	struct http_cookie_t* cookie;

	p = "LSID=DQAAAaem_vYg; Path=/accounts; Expires=Wed, 13 Jan 2021 22:23:01 GMT; Secure; HttpOnly";
	cookie = http_cookie_parse(p, strlen(p));
	assert(0==strcmp("LSID", http_cookie_get_name(cookie)) && 0==strcmp("DQAAAaem_vYg", http_cookie_get_value(cookie)));
	assert(0==strcmp("Wed, 13 Jan 2021 22:23:01 GMT", http_cookie_get_expires(cookie)) && 0==strcmp("/accounts", http_cookie_get_path(cookie)) && !http_cookie_get_domain(cookie) && 1==http_cookie_is_httponly(cookie) && 1==http_cookie_is_secure(cookie));
	http_cookie_destroy(cookie);

	p = "HSID=AYQEVKrdst; Domain=.foo.com; Path=/; Expires=Wed, 13 Jan 2021 22:23:01 GMT; HttpOnly";
	cookie = http_cookie_parse(p, strlen(p));
	assert(0==strcmp("HSID", http_cookie_get_name(cookie)) && 0==strcmp("AYQEVKrdst", http_cookie_get_value(cookie)));
	assert(0==strcmp("Wed, 13 Jan 2021 22:23:01 GMT", http_cookie_get_expires(cookie)) && 0==strcmp("/", http_cookie_get_path(cookie)) && 0==strcmp(".foo.com", http_cookie_get_domain(cookie)) && 1==http_cookie_is_httponly(cookie) && 0==http_cookie_is_secure(cookie));
	http_cookie_destroy(cookie);

	p = "SSID=Ap4GTEq; Domain=foo.com; Path=/; Expires=Wed, 13 Jan 2021 22:23:01 GMT; Secure; HttpOnly";
	cookie = http_cookie_parse(p, strlen(p));
	assert(0==strcmp("SSID", http_cookie_get_name(cookie)) && 0==strcmp("Ap4GTEq", http_cookie_get_value(cookie)));
	assert(0==strcmp("Wed, 13 Jan 2021 22:23:01 GMT", http_cookie_get_expires(cookie)) && 0==strcmp("/", http_cookie_get_path(cookie)) && 0==strcmp("foo.com", http_cookie_get_domain(cookie)) && 1==http_cookie_is_httponly(cookie) && 1==http_cookie_is_secure(cookie));
	http_cookie_destroy(cookie);

	p = "made_write_conn=1295214458; Path=/; Domain=.example.com";
	cookie = http_cookie_parse(p, strlen(p));
	assert(0==strcmp("made_write_conn", http_cookie_get_name(cookie)) && 0==strcmp("1295214458", http_cookie_get_value(cookie)));
	assert(!http_cookie_get_expires(cookie) && 0==strcmp("/", http_cookie_get_path(cookie)) && 0==strcmp(".example.com", http_cookie_get_domain(cookie)) && 0==http_cookie_is_httponly(cookie) && 0==http_cookie_is_secure(cookie));
	http_cookie_destroy(cookie);

	p = "name=value";
	cookie = http_cookie_parse(p, strlen(p));
	assert(0==strcmp("name", http_cookie_get_name(cookie)) && 0==strcmp("value", http_cookie_get_value(cookie)));
	assert(!http_cookie_get_expires(cookie) && !http_cookie_get_path(cookie) && !http_cookie_get_domain(cookie) && 0==http_cookie_is_httponly(cookie) && 0==http_cookie_is_secure(cookie));
	http_cookie_destroy(cookie);

	p = "name2=value2; Expires=Wed, 09 Jun 2021 10:18:14 GMT";
	cookie = http_cookie_parse(p, strlen(p));
	assert(0==strcmp("name2", http_cookie_get_name(cookie)) && 0==strcmp("value2", http_cookie_get_value(cookie)));
	assert(0==strcmp("Wed, 09 Jun 2021 10:18:14 GMT", http_cookie_get_expires(cookie)) && !http_cookie_get_path(cookie) && !http_cookie_get_domain(cookie) && 0==http_cookie_is_httponly(cookie) && 0==http_cookie_is_secure(cookie));
	http_cookie_destroy(cookie);
}

static void http_cookie_create_test(void)
{
	char cookie[128] = {0};
	assert(0==http_cookie_make(cookie, sizeof(cookie), "name", "value", NULL, NULL, NULL, 0, 0) && 0==strcmp(cookie, "name=value"));
	assert(0==http_cookie_make(cookie, sizeof(cookie), "name", "value", "/", NULL, NULL, 0, 0) && 0==strcmp(cookie, "name=value; path=/"));
	assert(0==http_cookie_make(cookie, sizeof(cookie), "name", "value", "/", "abc.com", NULL, 0, 0) && 0==strcmp(cookie, "name=value; path=/; domain=abc.com"));
	assert(0==http_cookie_make(cookie, sizeof(cookie), "name", "value", "/", "abc.com", "Wed, 09 Jun 2021 10:18:14 GMT", 0, 0) && 0==strcmp(cookie, "name=value; path=/; domain=abc.com; expires=Wed, 09 Jun 2021 10:18:14 GMT"));
	assert(0==http_cookie_make(cookie, sizeof(cookie), "name", "value", "/", "abc.com", "Wed, 09 Jun 2021 10:18:14 GMT", 1, 0) && 0==strcmp(cookie, "name=value; path=/; domain=abc.com; expires=Wed, 09 Jun 2021 10:18:14 GMT; HttpOnly"));
	assert(0==http_cookie_make(cookie, sizeof(cookie), "name", "value", "/", "abc.com", "Wed, 09 Jun 2021 10:18:14 GMT", 1, 1) && 0==strcmp(cookie, "name=value; path=/; domain=abc.com; expires=Wed, 09 Jun 2021 10:18:14 GMT; HttpOnly; Secure"));
	assert(0==http_cookie_make(cookie, sizeof(cookie), "name", "value", "/", NULL, NULL, 1, 1) && 0==strcmp(cookie, "name=value; path=/; HttpOnly; Secure"));
	assert(0==http_cookie_make(cookie, sizeof(cookie), "name", "value", NULL, NULL, NULL, 1, 0) && 0==strcmp(cookie, "name=value; HttpOnly"));
	assert(0==http_cookie_make(cookie, sizeof(cookie), "name", "value", "/", NULL, NULL, 0, 1) && 0==strcmp(cookie, "name=value; path=/; Secure"));
}

static void http_cookie_check_test(void)
{
}

void http_cookie_test(void)
{
	http_cookie_parse_test();
	http_cookie_create_test();
	http_cookie_check_test();
}
#endif
