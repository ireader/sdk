#include "http-header-host.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int http_header_host(const char* field, char host[], size_t bytes, unsigned short *port)
{
	size_t n;
	const char* p;

	if (!field || !host || bytes < 1)
		return -1;

	if ('[' == field[0])
	{
		field++; //skip '['
		p = strchr(field, ']');
		if (!p)
			return -1;

		n = (size_t)(p - field); // ptrdiff_t -> size_t
		p++;
	}
	else
	{
		p = strchr(field, ':');
		if (p)
		{
			// if more than one ':', assume to IPv6
			n = strchr(p+1, ':') ? strlen(field) : (size_t)(p - field); // ptrdiff_t -> size_t
			if (bytes <= n)
				return -1;
			p = field + n; // skip IPv6 port
		}
		else
		{
			n = strlen(field);
		}
	}

	if (bytes <= n)
		return -1;

	// copy host
	memcpy(host, field, n);
	host[n] = '\0';

	if (p && ':' == *p)
	{
		*port = (unsigned short)strtol(p + 1, &p, 10);
		if (0 != *p)
			return -1; // should be end with port
	}

	return 0;
}

#if defined(_DEBUG) || defined(DEBUG)
void http_header_host_test(void)
{
	char host[16] = {0};
	unsigned short port = 0;

	assert(0 == http_header_host("www.baidu.com", host, sizeof(host), &port) && 0==port && 0==strcmp("www.baidu.com", host));
	assert(0 == http_header_host("www.baidu.com:80", host, sizeof(host), &port) && 80==port && 0==strcmp("www.baidu.com", host));

	port = 0;
	assert(0 == http_header_host("114.21.2.11", host, sizeof(host), &port) && 0==port && 0==strcmp("114.21.2.11", host));
	assert(0 == http_header_host("114.21.2.11:8081", host, sizeof(host), &port) && 8081==port && 0==strcmp("114.21.2.11", host));
	assert(0 == http_header_host("[2001:db8::9:1]:8081", host, sizeof(host), &port) && 8081 == port && 0 == strcmp("2001:db8::9:1", host));
	assert(0 == http_header_host("2001:db8::9:1", host, sizeof(host), &port) && 0 == strcmp("2001:db8::9:1", host));
	
	assert(0 != http_header_host("www.verylongdnsname:com:80", host, sizeof(host), &port));
}
#endif
