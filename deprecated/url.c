#include "url.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "cstringext.h"
#include "urlcodec.h"

#define MAX_PARAMS	128
#define MAX_URL		2048

#define STRLEN(s)	(s?strlen(s):0)
#define FREE(p)		do{if(p){free(p); p=NULL;}}while(p)

typedef struct _url_param_t
{
	char* name;
	char* value;
} url_param_t;

typedef struct
{
	int port;
	char* scheme;
	char* host;
	char* path;

	url_param_t params[MAX_PARAMS];
	int count;

	char buffer[MAX_URL];
} url_t;

static int url_parse_param(const char* param, url_t* uri)
{
	const char *pn, *pv;
	url_param_t *pp;

	for(pn = param; param && *param && uri->count < MAX_PARAMS; pn=param+1)
	{
		param = strchr(pn, '&');
		pv = strchr(pn, '=');
		if(!pv || pv == pn || (param && pv>param)) // name is null
			continue;

		memset(uri->buffer, 0, sizeof(uri->buffer));

		pp = &uri->params[uri->count++];
		url_decode(pn, pv-pn, uri->buffer, sizeof(uri->buffer));
		pp->name = strdup(uri->buffer);

		++pv;
		if(param)
		{
			url_decode(pv, param-pv, uri->buffer, sizeof(uri->buffer));
		}
		else
		{
			url_decode(pv, -1, uri->buffer, sizeof(uri->buffer));
		}
		pp->value = strdup(uri->buffer);
	}
	return 0;
}

void* url_new()
{
	url_t* uri = (url_t*)malloc(sizeof(url_t));
	if(uri)
		memset(uri, 0, sizeof(url_t));
	return uri;
}

void url_free(void* id)
{
	int i;
	url_t* uri = (url_t*)id;
	if(!uri)
		return;

	for(i=0; i<uri->count; i++)
	{
		FREE(uri->params[i].name);
		FREE(uri->params[i].value);
	}
	FREE(uri->scheme);
	FREE(uri->host);
	FREE(uri->path);
	FREE(uri);
}

static int url_parse_host(const char* url, size_t len, url_t* uri)
{
	const char* p;
	p = strchr(url, ':');
	if(p && p<url+len)
	{
		uri->host = strndup(url, p-url);
		uri->port = atoi(p+1);
	}
	else
	{
		uri->host = strndup(url, len);
	}
	return 0;
}

// TODO: IPv6 url
void* url_parse(const char* url)
{
	size_t len;
	url_t* uri;
	const char *p;

	uri = (url_t*)url_new();
	if(!uri)
		return NULL;

	len = strlen(url);
	p = strchr(url, '/');
	if(!p)
	{
		url_parse_host(url, len, uri);
		uri->path = strdup("/"); // default to root
	}
	else
	{
		// scheme
		if(p>url && *(p-1)==':' && p[1]=='/' )
		{
			uri->scheme = strndup(url, p-1-url);

			len -= p + 2 - url;
			url = p + 2;
			p = strchr(url, '/');
		}

		// host and port
		if(!p)
		{
			url_parse_host(url, len, uri);
			uri->path = strdup("/"); // default to root
		}
		else
		{
			// "/web/index.html"
			if(p > url)
			{
				url_parse_host(url, p-url, uri);
				url = p;
			}

			// path
			p = strchr(url, '?');
			if(!p)
			{
				uri->path = strdup(url);
			}
			else
			{
				uri->path = strndup(url, p-url);

				// param
				url_parse_param(p+1, uri);
			}
		}
	}

	return uri;
}

int url_geturlpath(void* id, char* url, size_t len)
{
	int i, offset;

	url_t* uri = (url_t*)id;

	offset = 0;
	if (uri->path)
	{
		assert('/' == uri->path[0]);
		offset += strlcpy(url + offset, uri->path, len - offset);
	}
	else
	{
		offset += strlcpy(url + offset, "/", len - offset);
	}

	for (i = 0; i < uri->count; i++)
	{
		offset += strlcat(url + offset, 0 == i ? "?" : "&", len - offset);

		url_encode(uri->params[i].name, -1, uri->buffer, sizeof(uri->buffer));
		offset += strlcat(url + offset, uri->buffer, len - offset);
		offset += strlcat(url + offset, "=", len - offset);
		url_encode(uri->params[i].value, -1, uri->buffer, sizeof(uri->buffer));
		offset += strlcat(url + offset, uri->buffer, len - offset);
	}

	return 0;
}

int url_geturl(void* id, char* url, size_t len)
{
	size_t n, offset;
	url_t* uri = (url_t*)id;
	if(!uri || !url || !uri->host)
		return -1;

	n = STRLEN(uri->scheme) + STRLEN(uri->host) + STRLEN(uri->path) + uri->count + 8;
	if(len < n)
		return n;

	offset = 0;
	if(uri->scheme)
	{
		offset += strlcpy(url + offset, uri->scheme, len-offset);
		offset += strlcat(url + offset, "://", len - offset);
	}

	if(uri->host)
		offset += strlcat(url + offset, uri->host, len - offset);

	if(uri->port)
		offset += snprintf(url + offset, len - offset, ":%d", uri->port);

	return offset + url_geturlpath(id, url + offset, len - offset);
}

int url_setscheme(void* id, const char* scheme)
{
	url_t* uri = (url_t*)id;
	if(!uri || !scheme)
		return -1;

	FREE(uri->scheme);
	uri->scheme = strdup(scheme);
	return uri->scheme?errno:0;
}

int url_sethost(void* id, const char* host)
{
	url_t* uri = (url_t*)id;
	if(!uri || !host)
		return -1;

	FREE(uri->host);
	uri->host = strdup(host);
	return uri->host?errno:0;
}

int url_setpath(void* id, const char* path)
{
	url_t* uri = (url_t*)id;
	if(!uri || !path)
		return -1;

	FREE(uri->path);
	uri->path = strdup(path);
	return uri->path?errno:0;
}

int url_setport(void* id, int port)
{
	url_t* uri = (url_t*)id;
	if(!uri)
		return -1;

	uri->port = port;
	return 0;
}

const char* url_getscheme(void* id)
{
	url_t* uri = (url_t*)id;
	if(!uri)
		return NULL;
	return uri->scheme;
}

const char* url_gethost(void* id)
{
	url_t* uri = (url_t*)id;
	if(!uri)
		return NULL;
	return uri->host;
}

const char* url_getpath(void* id)
{
	url_t* uri = (url_t*)id;
	if(!uri)
		return NULL;
	return uri->path;
}

int url_getport(void* id)
{
	url_t* uri = (url_t*)id;
	if(!uri)
		return -(int)EPERM;
	return uri->port;
}

int url_setparam(void* id, const char* name, const char* value)
{
	url_param_t* pp;

	url_t* uri = (url_t*)id;
	if(!uri)
		return -(int)EPERM;

	pp = &uri->params[uri->count++];
	pp->name = strdup(name);
	pp->value = strdup(value);
	return 0;
}

int url_getparam_count(void* id)
{
	url_t* uri = (url_t*)id;
	if(!uri)
		return -(int)EPERM;
	return uri->count;
}

int url_getparam(void* id, int index, const char** name, const char** value)
{
	url_t* uri = (url_t*)id;
	if(!uri || index<0 || index>=uri->count)
		return -(int)EPERM;

	if(name)
		*name = uri->params[index].name;
	if(value)
		*value = uri->params[index].value;
	return 0;
}
