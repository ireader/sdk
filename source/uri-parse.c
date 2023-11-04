// RFC3986 Uniform Resource Identifier (URI): Generic Syntax => 2. Characters (p11)
// unreserved = ALPHA / DIGIT / "-" / "." / "_" / "~"
// reserved = gen-delims / sub-delims
// gen-delims = ":" / "/" / "?" / "#" / "[" / "]" / "@"
// sub-delims = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="

#include "uri-parse.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

/*
0000 0000 0000 0000 0000 0000 0000 0000

?>=< ;:98 7654 3210 /.-, +*)( '&%$ #"! 
1010 1111 1111 1111 1111 1111 1111 1010

_^]\ [ZYX WVUT SRQP ONML KJIH GFED CBA@
1010 1111 1111 1111 1111 1111 1111 1111

 ~}| {zyx wvut srqp onml kjih gfed cba`
0100 0111 1111 1111 1111 1111 1111 1110

0000 0000 0000 0000 0000 0000 0000 0000
0000 0000 0000 0000 0000 0000 0000 0000
0000 0000 0000 0000 0000 0000 0000 0000
0000 0000 0000 0000 0000 0000 0000 0000
*/
static uint32_t s_characters[8] = { 0x00, 0xAFFFFFFA, 0xAFFFFFFF, 0x47FFFFFE, 0x00, 0x00, 0x00, 0x00 };

static int uri_characters_check(uint8_t c)
{
	int n = c / 32;
	int m = c % 32;
	return s_characters[n] & (1 << m);
}

static int uri_parse_complex(struct uri_t* uri, const char* str, int len);

struct uri_t* uri_parse(const char* uri, int len)
{
	struct uri_t* u;

	if (NULL == uri || 0 == *uri || len < 1)
		return NULL;

	u = (struct uri_t*)malloc(sizeof(*u) + len + 5);
	if (NULL == u)
		return NULL;

	if (0 != uri_parse_complex(u, uri, len))
	{
		free(u);
		return NULL;
	}

	return u;
}

void uri_free(struct uri_t* uri)
{
	if(uri) free(uri);
}

struct uri_component_t
{
	int has_scheme;
	int has_userinfo;
	int has_host;
	int has_port;
};

static int uri_check(const char* str, int len, struct uri_component_t* items)
{
	char c;
	const char* pend;
	enum
	{
		URI_PARSE_START,
		URI_PARSE_SCHEME,
		URI_PARSE_AUTHORITY,
		URI_PARSE_USERINFO,
		URI_PARSE_HOST,
		URI_PARSE_PORT,

		URI_PARSE_HOST_IPV6,
	} state;

	state = URI_PARSE_START;
	items->has_scheme = 0;
	items->has_userinfo = 0;
	items->has_host = 0;
	items->has_port = 0;

	for (pend = str + len; str < pend; ++str)
	{
		c = *str;

		if (0 == uri_characters_check(c))
			return -1; // invalid character

		switch (state)
		{
		case URI_PARSE_START:
			switch (c)
			{
			case '/':
				// path only, all done
				return 0; // ok

			case '[':
				state = URI_PARSE_HOST_IPV6;
				items->has_host = 1;
				break;

			default:
				state = URI_PARSE_SCHEME;
				items->has_host = 1;
				--str; // go back
				break;
			}
			break;

		case URI_PARSE_SCHEME:
			switch (c)
			{
			case ':':
				state = URI_PARSE_AUTHORITY;
				break;

			case '@':
				state = URI_PARSE_HOST;
				items->has_userinfo = 1;
				break;

			case '/':
			case '?':
			case '#':
				// all done, host only
				return 0;

			default:
				break;
			}
			break;

		case URI_PARSE_AUTHORITY:
			if ('/' == c)
			{
				if (str + 1 < pend && '/' == str[1])
				{
					state = URI_PARSE_HOST;
					items->has_scheme = 1;
					str += 1; // skip "//"
				}
				else
				{
					items->has_port = 1;
					return 0;
				}
			}
			else
			{
				items->has_port = 1;
				state = URI_PARSE_PORT;
			}
			break;

		case URI_PARSE_HOST:
			assert(']' != c);
			switch (c)
			{
			case '@':
				items->has_userinfo++;
				//state = URI_PARSE_HOST;
				break;

			case '[':
				state = URI_PARSE_HOST_IPV6;
				break;

			case ':':
				items->has_port = 1;
				state = URI_PARSE_PORT;
				break;

			case '/':
			case '?':
			case '#':
				return 0;

			default:
				break;
			}
			break;

		case URI_PARSE_PORT:
			switch (c)
			{
			case '@':
				items->has_port = 0;
				items->has_userinfo++;
				state = URI_PARSE_HOST;
				break;

			case '[':
			case ']':
			case ':':
				return -1;

			case '/':
			case '?':
			case '#':
				items->has_port = 1;
				return 0;

			default:
				break;
			}
			break;

		case URI_PARSE_HOST_IPV6:
			switch (c)
			{
			case ']':
				state = URI_PARSE_HOST;
				break;

			case '@':
			case '[':
			case '/':
			case '?':
			case '#':
				return -1;

			default:
				break;
			}
			break;

		default:
			assert(0);
			return -1;
		}
	}

	return 0;
}

static int uri_parse_complex(struct uri_t* uri, const char* str, int len)
{
	char *p;
	const char* pend;
	struct uri_component_t items;

	if (0 != uri_check(str, len, &items))
		return -1; // invalid uri

	pend = str + len;
	p = (char*)(uri + 1);

	// scheme
	if (items.has_scheme)
	{
		uri->scheme = p;
		while (str < pend && ':' != *str)
			*p++ = *str++;
		*p++ = 0;
		str += 3; // skip "://"
	}
	else
	{
		uri->scheme = NULL;
	}

	// user info
	if (items.has_userinfo)
	{
		uri->userinfo = p;
		// fix password have many '@', e.g. rtsp://admin:123@456@789@192.168.1.100/live/camera1
		while (str < pend && ('@' != *str || --items.has_userinfo > 0))
			*p++ = *str++;
		*p++ = 0;
		str += 1; // skip "@"
	}
	else
	{
		uri->userinfo = NULL;
	}

	// host
	if (items.has_host)
	{
		uri->host = p;
		assert(str < pend);
		if ('[' == *str)
		{
			// IPv6
			++str;
			while (str < pend && ']' != *str)
				*p++ = *str++;
			*p++ = 0;
			str += 1; // skip "]"

			if (str < pend && *str && NULL == strchr(":/?#", *str))
				return -1;
		}
		else
		{
			while (str < pend && *str && NULL == strchr(":/?#", *str))
				*p++ = *str++;
			*p++ = 0;
		}
	}
	else
	{
		uri->host = NULL;
	}

	// port
	if (items.has_port)
	{
		++str; // skip ':'
		for (uri->port = 0; str < pend && *str >= '0' && *str <= '9'; str++)
			uri->port = uri->port * 10 + (*str - '0');

		if (str < pend && *str && NULL == strchr(":/?#", *str))
			return -1;
	}
	else
	{
		uri->port = 0;
	}

	// 3.3. Path (p22)
	// The path is terminated by the first question mark ("?") 
	// or number sign ("#") character, 
	// or by the end of the URI.
	uri->path = p; // awayls have path(default '/')
	if (str < pend && '/' == *str)
	{
		while (str < pend && *str && '?' != *str && '#' != *str)
			*p++ = *str++;
		*p++ = 0;
	}
	else
	{
		// default path
		*p++ = '/';
		*p++ = 0;
	}

	// 3.4. Query (p23)
	// The query component is indicated by the first question mark ("?") character 
	// and terminated by a number sign ("#") character
    // or by the end of the URI.
	if (str < pend && '?' == *str)
	{
		uri->query = p;
		for (++str; str < pend && *str && '#' != *str; ++str)
			*p++ = *str;
		*p++ = 0;
	}
	else
	{
		uri->query = NULL;
	}

	// 3.5. Fragment
	if (str < pend && '#' == *str)
	{
		uri->fragment = p;
		while (str < pend && *++str)
			*p++ = *str;
		*p++ = 0;
	}
	else
	{
		uri->fragment = NULL;
	}

	return 0;
}

int uri_path(const struct uri_t* uri, char* buf, int len)
{
    int r, n;
    n = snprintf(buf, len, "%s", uri->path);
    if(n < 0 || n >= len)
        return -1;
    
    if(uri->query && *uri->query)
    {
        r = snprintf(buf + n, len - n, "?%s", uri->query);
        if(r < 0 || r + n >= len)
            return -1;
        n += r;
    }

    if(uri->fragment && *uri->fragment)
    {
        r = snprintf(buf + n, len - n, "#%s", uri->fragment);
        if(r < 0 || r + n >= len)
            return -1;
        n += r;
    }
    
    return n;
}

int uri_userinfo(const struct uri_t* uri, char* usr, int n1, char* pwd, int n2)
{
    const char* sep;
    if(!uri->userinfo)
    {
        usr[0] = 0;
        pwd[0] = 0;
    }
    else
    {
        sep = strchr(uri->userinfo, ':');
        if(sep)
        {
            snprintf(usr, n1, "%.*s", (int)(sep - uri->userinfo), uri->userinfo);
            snprintf(pwd, n2, "%s", sep + 1);
        }
        else
        {
            snprintf(usr, n1, "%s", uri->userinfo);
            pwd[0] = 0;
        }
    }
    
    return 0;
}
