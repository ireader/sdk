#include "http-client.h"
#include "uri-parse.h"
#include "sys/sock.h"
#include "sys/path.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

struct http_download_t
{
	struct uri_t* uri;
	http_client_t* http;
};

static const char* http_path(const char* url)
{
	const char* p;
	p = strchr(url, '/');
	if (p > url && ':' == *(p - 1))
		p = strchr(p + 2, '/');
	return p ? p : url;
}

static void http_ondownload(void* fp, int code, void* buf, size_t len)
{
    fwrite(buf, 1, len, (FILE*)fp);
}

static void http_onreply(void* param, int code, int http_status_code, int64_t http_content_length)
{
    static char buf[64 * 1024];
    
	FILE* fp;
    const char* name;
	struct http_download_t* http;
	http = (struct http_download_t*)param;
	name = path_basename(http->uri->path);

	if (0 == code && name)
	{
        assert(http_content_length > 0 && http_content_length < sizeof(buf));
		fp = fopen(name, "wb");
		if (fp)
		{
			http_client_read(http->http, buf, http_content_length, HTTP_READ_FLAGS_WHOLE, http_ondownload, fp);
			fclose(fp);
			printf("http download: %s, bytes: %u\n", name, (unsigned int)http_content_length);
		}
	}
	else
	{
		printf("http download failed: %d\n", code);
	}
}

// http://www.tiantangbt.com/d/file/suspense/2017-10-01/The.Invisible.Guest.2016.720p.BluRay.x264-USURY706178.torrent
void http_download(const char* url)
{
	struct http_download_t http;
	struct http_header_t headers[3];	

	headers[0].name = "User-Agent";
	headers[0].value = "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:34.0) Gecko/20100101 Firefox/34.0";
	headers[1].name = "Accept-Language";
	headers[1].value = "en-US,en;q=0.5";
	headers[2].name = "Connection";
	headers[2].value = "keep-alive";

	http.uri = uri_parse(url, strlen(url));
	assert(http.uri);

	socket_init();
	http.http = http_client_create(NULL, http.uri->scheme, http.uri->host, http.uri->port ? http.uri->port : 80);
	http_client_get(http.http, http_path(url), headers, sizeof(headers) / sizeof(headers[0]), http_onreply, &http);
	http_client_destroy(http.http);
	uri_free(http.uri);
	socket_cleanup();
}
