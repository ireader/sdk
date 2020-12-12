#include "uri-parse.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void uri_standard()
{
	struct uri_t *uri;
	const char* str;

	str = "http://www.~abcdefghijklmnopqrstuvwxyz-0123456789_ABCDEFGHIJKLMNOPQRSTUVWXYZ!$&'()*+,;=.com";
	uri = uri_parse(str, strlen(str));
	assert(0 == uri->userinfo && 0 == uri->query && 0 == uri->fragment && 0 == uri->port);
	assert(0 == strcmp("www.~abcdefghijklmnopqrstuvwxyz-0123456789_ABCDEFGHIJKLMNOPQRSTUVWXYZ!$&'()*+,;=.com", uri->host));
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("/", uri->path));
	uri_free(uri);

	str = "http://www.microsoft.com:80";
	uri = uri_parse(str, strlen(str));
	assert(0 == uri->userinfo && 0 == uri->query && 0 == uri->fragment);
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("/", uri->path));
	assert(80 == uri->port);
	uri_free(uri);

	str = "http://www.microsoft.com:80/";
	uri = uri_parse(str, strlen(str));
	assert(0 == uri->userinfo && 0 == uri->query && 0 == uri->fragment);
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("/", uri->path));
	assert(80 == uri->port);
	uri_free(uri);

	str = "http://usr:pwd@www.microsoft.com";
	uri = uri_parse(str, strlen(str));
	assert(0 == uri->query && 0 == uri->fragment && 0 == uri->port);
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("usr:pwd", uri->userinfo));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/", uri->path));
	uri_free(uri);

	str = "http://usr:pwd@www.microsoft.com/";
	uri = uri_parse(str, strlen(str));
	assert(0 == uri->query && 0 == uri->fragment && 0 == uri->port);
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("usr:pwd", uri->userinfo));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/", uri->path));
	uri_free(uri);

	str = "http://usr:pwd@www.microsoft.com:80";
	uri = uri_parse(str, strlen(str));
	assert(0 == uri->query && 0 == uri->fragment);
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("usr:pwd", uri->userinfo));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/", uri->path));
	assert(80 == uri->port);
	uri_free(uri);

	str = "http://usr:pwd@www.microsoft.com:80/";
	uri = uri_parse(str, strlen(str));
	assert(0 == uri->query && 0 == uri->fragment);
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("usr:pwd", uri->userinfo));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/", uri->path));
	assert(80 == uri->port);
	uri_free(uri);

	// with path
	str = "http://www.microsoft.com/china/";
	uri = uri_parse(str, strlen(str));
	assert(0 == uri->userinfo && 0 == uri->query && 0 == uri->fragment && 0 == uri->port);
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("/china/", uri->path));
	uri_free(uri);

	str = "http://www.microsoft.com/china/default.html";
	uri = uri_parse(str, strlen(str));
	assert(0 == uri->userinfo && 0 == uri->query && 0 == uri->fragment && 0 == uri->port);
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/china/default.html", uri->path));
	uri_free(uri);

	// with param
	str = "http://www.microsoft.com/china/default.html?encoding=utf-8";
	uri = uri_parse(str, strlen(str));
	assert(0 == uri->userinfo && 0 == uri->fragment && 0 == uri->port);
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/china/default.html", uri->path));
	assert(0 == strcmp("encoding=utf-8", uri->query));
	uri_free(uri);

	str = "http://www.microsoft.com/china/default.html?encoding=utf-8&font=small";
	uri = uri_parse(str, strlen(str));
	assert(0 == uri->userinfo && 0 == uri->fragment && 0 == uri->port);
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/china/default.html", uri->path));
	assert(0 == strcmp("encoding=utf-8&font=small", uri->query));
	uri_free(uri);

	str = "http://usr:pwd@www.microsoft.com:80/china/default.html?encoding=utf-8&font=small#tag";
	uri = uri_parse(str, strlen(str));
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("usr:pwd", uri->userinfo));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/china/default.html", uri->path));
	assert(0 == strcmp("encoding=utf-8&font=small", uri->query));
	assert(0 == strcmp("tag", uri->fragment));
	assert(80 == uri->port);
	char usr[64], pwd[64];
	assert(0 == uri_userinfo(uri, usr, sizeof(usr), pwd, sizeof(pwd)) && 0 == strcmp(usr, "usr") && 0 == strcmp(pwd, "pwd"));
	uri_free(uri);
}

static void uri_without_scheme()
{
	struct uri_t *uri;
	const char* str;

	str = "www.microsoft.com";
	uri = uri_parse(str, strlen(str));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/", uri->path));
	uri_free(uri);

	str = "www.microsoft.com:80";
	uri = uri_parse(str, strlen(str));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/", uri->path));
	assert(80 == uri->port);
	uri_free(uri);

	str = "www.microsoft.com:80/";
	uri = uri_parse(str, strlen(str));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/", uri->path));
	assert(80 == uri->port);
	uri_free(uri);

	str = "usr:pwd@www.microsoft.com";
	uri = uri_parse(str, strlen(str));
	assert(0 == uri->scheme && 0 == uri->query && 0 == uri->fragment && 0 == uri->port);
	assert(0 == strcmp("usr:pwd", uri->userinfo));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/", uri->path));
	uri_free(uri);

	str = "usr:pwd@www.microsoft.com/";
	uri = uri_parse(str, strlen(str));
	assert(0 == uri->scheme && 0 == uri->query && 0 == uri->fragment && 0 == uri->port);
	assert(0 == strcmp("usr:pwd", uri->userinfo));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/", uri->path));
	uri_free(uri);

	// with path
	str = "www.microsoft.com/china/";
	uri = uri_parse(str, strlen(str));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/china/", uri->path));
	uri_free(uri);

	str = "www.microsoft.com/china/default.html";
	uri = uri_parse(str, strlen(str));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/china/default.html", uri->path));
	uri_free(uri);

	// with param
	str = "www.microsoft.com/china/default.html?encoding=utf-8";
	uri = uri_parse(str, strlen(str));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/china/default.html", uri->path));
	assert(0 == strcmp("encoding=utf-8", uri->query));
	uri_free(uri);

	str = "www.microsoft.com/china/default.html?encoding=utf-8&font=small";
	uri = uri_parse(str, strlen(str));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/china/default.html", uri->path));
	assert(0 == strcmp("encoding=utf-8&font=small", uri->query));
	uri_free(uri);

	str = "usr:pwd@www.microsoft.com:80/china/default.html?encoding=utf-8&font=small#tag";
	uri = uri_parse(str, strlen(str));
	assert(0 == uri->scheme);
	assert(0 == strcmp("usr:pwd", uri->userinfo));
	assert(0 == strcmp("www.microsoft.com", uri->host));
	assert(0 == strcmp("/china/default.html", uri->path));
	assert(0 == strcmp("encoding=utf-8&font=small", uri->query));
	assert(0 == strcmp("tag", uri->fragment));
	assert(80 == uri->port);
	uri_free(uri);
}

static void uri_without_host()
{
	struct uri_t *uri;
	const char* str;

	str = "/china/default.html";
	uri = uri_parse(str, strlen(str));
	assert(!uri->scheme && !uri->userinfo && !uri->host && !uri->query && !uri->fragment);
	assert(0 == strcmp("/china/default.html", uri->path));
	uri_free(uri);

	str = "/china/default.html?encoding=utf-8&font=small";
	uri = uri_parse(str, strlen(str));
	assert(!uri->scheme && !uri->userinfo && !uri->host && !uri->fragment);
	assert(0 == strcmp("/china/default.html", uri->path));
	assert(0 == strcmp("encoding=utf-8&font=small", uri->query));
	uri_free(uri);

	str = "/china/default.html?encoding=utf-8&font=small#tag";
	uri = uri_parse(str, strlen(str));
	assert(!uri->scheme && !uri->userinfo && !uri->host);
	assert(0 == strcmp("/china/default.html", uri->path));
	assert(0 == strcmp("encoding=utf-8&font=small", uri->query));
	assert(0 == strcmp("tag", uri->fragment));
	uri_free(uri);
}

static void uri_ipv6()
{
	struct uri_t *uri;
	const char* str;

	str = "[::1]";
	uri = uri_parse(str, strlen(str));
	assert(0 == strcmp("::1", uri->host));
	assert(0 == strcmp("/", uri->path));
	uri_free(uri);

	str = "[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:80/index.html";
	uri = uri_parse(str, strlen(str));
	assert(!uri->query && !uri->userinfo && !uri->fragment);
	assert(0 == strcmp("FEDC:BA98:7654:3210:FEDC:BA98:7654:3210", uri->host));
	assert(0 == strcmp("/index.html", uri->path));
	assert(80 == uri->port);
	uri_free(uri);

	str = "usr:pwd@[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]/index.html";
	uri = uri_parse(str, strlen(str));
	assert(!uri->query && !uri->fragment);
	assert(0 == strcmp("usr:pwd", uri->userinfo));
	assert(0 == strcmp("FEDC:BA98:7654:3210:FEDC:BA98:7654:3210", uri->host));
	assert(0 == strcmp("/index.html", uri->path));
	uri_free(uri);

	str = "http://[::1]";
	uri = uri_parse(str, strlen(str));
	assert(0 == strcmp("::1", uri->host));
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("/", uri->path));
	uri_free(uri);

	str = "http://[fe80::1%2511]";
	uri = uri_parse(str, strlen(str));
	assert(0 == strcmp("fe80::1%2511", uri->host));
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("/", uri->path));
	uri_free(uri);

	str = "http://[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:80/index.html";
	uri = uri_parse(str, strlen(str));
	assert(!uri->query && !uri->userinfo && !uri->fragment);
	assert(0 == strcmp("FEDC:BA98:7654:3210:FEDC:BA98:7654:3210", uri->host));
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("/index.html", uri->path));
	assert(80 == uri->port);
	uri_free(uri);

	str = "http://usr:pwd@[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:80/index.html";
	uri = uri_parse(str, strlen(str));
	assert(!uri->query && !uri->fragment);
	assert(0 == strcmp("http", uri->scheme));
	assert(0 == strcmp("usr:pwd", uri->userinfo));
	assert(0 == strcmp("FEDC:BA98:7654:3210:FEDC:BA98:7654:3210", uri->host));
	assert(0 == strcmp("/index.html", uri->path));
	assert(80 == uri->port);
	uri_free(uri);
}

static void uri_character_test(void)
{
	char s[64];
	unsigned char c;
	struct uri_t *uri;

	for (c = 1; c < 255; c++)
	{
		if(strchr("~-_.abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!$&'()*+,;=:/?#[]@%", c))
			continue;

		snprintf(s, sizeof(s), "http://host%c", c);
		uri = uri_parse(s, strlen(s));
		assert(NULL == uri);
	}
}

void uri_parse_test(void)
{
	struct uri_t *uri;
	uri = uri_parse("", 0);
	assert(NULL == uri);

	uri = uri_parse("/", 1);
	assert(!uri->scheme && !uri->host && !uri->userinfo && !uri->query && !uri->fragment);
	assert(0 == strcmp("/", uri->path));
	uri_free(uri);

	uri_character_test();
	uri_standard();
	uri_without_scheme();
	uri_without_host();
	uri_ipv6();
}
