#include "http-client-internal.h"
#include "http-transport.h"
#include "http-cookie.h"
#include "cstringext.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int http_client_request(struct http_client_t *http, int method, const char* uri, const struct http_header_t *headers, size_t n, const void* msg, size_t bytes, http_client_onresponse onreply, void* param);

static int http_client_setcookie(struct http_client_t* http, const char* cookie, size_t bytes)
{
	if(http->ncookie < bytes+1)
	{
		char* p;
		p = (char*)realloc(http->cookie, bytes+1);
		if(!p)
			return ENOMEM;
		http->cookie = p;
		http->ncookie = bytes+1;
	}

	memcpy(http->cookie, cookie, bytes);
	http->cookie[bytes] = '\0';
	return 0;
}

static int http_client_cookie_handler(struct http_client_t* http)
{
    int len;
    char buffer[512];
    const char* cookie;

    // FIXME:
    // 1. only handle one cookie item
    // 2. check cookie path
    // 3. check cookie expire
    // 4. check cookie secure

//    for(i = 0; i < n; i++){}
    
    // handle cookie
    cookie = http_get_cookie(http->parser);
    if(cookie)
    {
        http_cookie_t* ck;
        ck = http_cookie_parse(cookie, strlen(cookie));
        if(ck)
        {
            const char *cookiename, *cookievalue;
            cookiename = http_cookie_get_name(ck);
            cookievalue = http_cookie_get_value(ck);
            if (cookiename && cookievalue)
            {
                len = snprintf(buffer, sizeof(buffer), "%s=%s", cookiename, cookievalue);
                http_client_setcookie(http, buffer, len);
                assert(len + 1 < sizeof(buffer));
            }
            http_cookie_destroy(ck);
        }
    }
    
    return 0;
}

static void http_client_redirect_url_clean(struct http_client_t* http)
{
    int i;
    for(i = 1; i < http->redirect.n; i++)
        free(http->redirect.urls[i]);
    http->redirect.n = 0;
}

static int http_client_redirect_handler(struct http_client_t* http)
{
    const char* uri;
    
    if(http->redirect.n >= sizeof(http->redirect.urls)/sizeof(http->redirect.urls[0]))
        return 0; // too many redirect
    
    // redirect
    uri = http_get_header_by_name(http->parser, "Location");
    if(!uri || !*uri)
        return 0;
    
    // TODO: base url -> path url
    //path_resolve(http->redirect.urls[0], uri, fullpath);
    
    http->redirect.urls[http->redirect.n++] = strdup(uri);
    if(http->redirect.onredirect)
        return http->redirect.onredirect(http->redirect.param, http->redirect.urls, http->redirect.n);
    return 1; // enable redirect
}

// POST http://ip:port/xxx HTTP/1.1\r\n
// HOST: ip:port\r\n
// Content-Length: bytes\r\n
// Content-Type: application/x-www-form-urlencoded\r\n
static int http_make_request(struct http_client_t *http, int method, const char* uri, const struct http_header_t *headers, size_t n, size_t bytes)
{
	int r;
	size_t i, nc, vc;
	int flag_host = 0;
	int flag_cookie = 0;
	int flag_content_length = 0;

	// Request Line
	r = http_request_set_uri(http->req, method, uri);

	// User-defined headers
	for (i = 0; i < n; i++)
	{
		nc = headers[i].name ? strlen(headers[i].name) : 0;
		vc = headers[i].value ? strlen(headers[i].value) : 0;
		if (nc < 1) continue; // invalid header name

		if (0 == strcasecmp("Content-Length", headers[i].name))
		{
			flag_content_length = 1;
		}
		else if(0 == strcasecmp("HOST", headers[i].name))
		{
			flag_host = 1;
		}
		else if(0 == strcasecmp("Cookie", headers[i].name))
		{
			flag_cookie = 1;
		}

		r = http_request_set_header(http->req, headers[i].name, headers[i].value);
		if (0 != r)
			break;
	}

	// Add Other Header
	if (0 == r && 0 == flag_host)
		r = http_request_set_host(http->req, http->host, http->port);
	if (0 == r && 0 == flag_cookie && http->cookie)
		r = http_request_set_header(http->req, "Cookie", http->cookie);
	if (0 == r && 0 == flag_content_length)
		r = http_request_set_header_int(http->req, "Content-Length", (int)bytes);
	return r;
}

void http_client_handle(struct http_client_t *http, int code)
{
    // Connection: close
    if (code < 0 ||  0 == http->status || 1 == http_get_connection(http->parser))
    {
//        http->conn->destroy(http->connection, http_client_transport_ondestroy);
    }
    
//    if (1 == aio->retry)
//    {
//        // try resend
//        aio->retry = 0;
//        code = aio_client_send_v(aio->client, aio->vec, aio->count);
//    }
//    else
//    {
//        code = ECONNRESET; // peer close
//    }
    
	locker_lock(&http->locker);
	if(http->onreply)
        http->onreply(http->cbparam, code, 0 == code ? http_get_status_code(http->parser) : 0, 0==code ? http_get_content_length(http->parser) : 0);
	locker_unlock(&http->locker);

	http_client_release(http);
}

static void http_client_onbody(void* param, const void* buf, int len)
{
    uint8_t* ptr;
    http_client_t* http;
    http = (http_client_t*)param;

    if(http->body.off == http->body.len)
    {
        http->body.off = 0;
        http->body.len = 0;
    }
    
    // TODO: don't copy for many chunks
    
    if(http->body.len + len > http->body.cap)
    {
        ptr = (uint8_t*)realloc(http->body.ptr, http->body.cap + len);
        if(!ptr)
            return;
        
        http->body.ptr = ptr;
        http->body.cap += len;
    }
    
    memcpy(http->body.ptr + http->body.len, buf, len);
    http->body.len += len;
}

static void http_client_onread_body(void* param, int code, const void* buf, int len);
static int http_client_dobody(http_client_t* http, int *needmore)
{
    int n;
    if(http->body.off < http->body.len)
    {
        n = http->body.len - http->body.off;
        n = (http->body.bytes - http->body.readed) > n ? n : (http->body.bytes - http->body.readed);
        memcpy(http->body.data + http->body.readed, http->body.ptr + http->body.off, n);
        http->body.readed += n;
        http->body.off += n;
        
        if(0 == (http->body.flags & HTTP_READ_FLAGS_WHOLE) || http->body.readed == http->body.bytes)
        {
            http->body.onread(http->body.param, 0, http->body.data, http->body.readed);
            return 0;
        }
    }
    
    if(0 == http->status)
    {
        http->body.onread(http->body.param, 0, http->body.data, http->body.readed);
        return 0;
    }
    
    // need more data
    assert(http->body.off == http->body.len);
    *needmore = http->transport->is_aio ? 0 : 1;
    return http->transport->is_aio ? http->transport->recv(http->connection, http_client_onread_body, http) : 0;
}

static void http_client_onread_body(void* param, int code, const void* buf, int len)
{
    int f;
    size_t n;
    http_client_t* http;
    http = (http_client_t*)param;

    if (0 == code)
    {
        n = len;
        http->status = http_parser_input(http->parser, buf, &n);
        if(http->status < 0)
        {
           // http parse error
           code = http->status;
        }
        else
        {
            assert(0 == http->status || 2 == http->status);
            code = http->transport->is_aio ? http_client_dobody(http, &f) : 0;
        }
    }
    
    if(0 != code)
    {
        http->body.onread(http->body.param, code > 0 ? -code : code, http->body.data, http->body.readed);
        
        //TODO: disconnection
    }
}

static void http_client_onread_header(void* param, int code, const void* buf, int len)
{
    size_t n;
    http_client_t* http;
    http = (http_client_t*)param;

    if (0 == code)
    {
        n = len;
        http->status = http_parser_input(http->parser, buf, &n);
        if(http->status < 0)
        {
            // http parse error
            code = http->status;
        }
        else if(1 == http->status)
        {
            // need more data
            code = http->transport->is_aio ? http->transport->recv(http->connection, http_client_onread_header, http) : 0;
        }
        else
        {
            assert(0 == http->status || 2 == http->status);
            http_client_cookie_handler(http);
            
            code = http_get_status_code(http->parser);
            if(300 <= code && code < 400)
            {
                if(http_client_redirect_handler(http))
                {
                    code = http_client_request(http, http->parameters.method, http->parameters.uri, http->parameters.headers, http->parameters.n, http->parameters.msg, http->parameters.bytes, http->onreply, http->cbparam);
                    if(0 != code)
                        http_client_handle(http, code > 0 ? -code : code);
                    return;
                }
            }
            
            assert(0 == n);
            http_client_handle(http, 0);
            return;
        }
    }
    
    if(0 != code)
    {
        http_client_handle(http, code > 0 ? -code : code);
    }
}

static void http_client_onsend(void* param, int code)
{
    http_client_t* http;
    http = (http_client_t*)param;
    if(0 == code)
    {
        // break Block-IO Depth Callback
        do
        {
            code = http->transport->recv(http->connection, http_client_onread_header, http);
        } while(0 == code && 1 == http->status && http->transport->is_aio);
    }
    
    if(0 != code)
    {
        http_client_handle(http, code > 0 ? -code : code);
    }
}

static int http_client_request(struct http_client_t *http, int method, const char* uri, const struct http_header_t *headers, size_t n, const void* msg, size_t bytes, http_client_onresponse onreply, void* param)
{
	int r, len;
	const char* header;

    http->parameters.method = method;
    http->parameters.uri = uri;
    http->parameters.headers = headers;
    http->parameters.n = n;
    http->parameters.msg = msg;
    http->parameters.bytes = bytes;
    
    http->cbparam = param;
    http->onreply = onreply;
    http->tryagain = 0;
    http->body.len = http->body.off = 0; // reset
    
    // clear status
    http_parser_clear(http->parser);
    
    if(http->connection)
        http->transport->close(http->connection);
    http->connection = http->transport->connect(http->transport, http->scheme, http->host, http->port);
    if(!http->connection)
        return -1;
    
	if(0 != http_make_request(http, method, uri, headers, n, bytes))
        return -1;
    
	header = http_request_get(http->req, &len);

    atomic_increment32(&http->ref);
    r = http->transport->send(http->connection, header, len, msg, (int)bytes, http_client_onsend, http);
    if (0 != r)
		http_client_release(http);
    
	return r;
}

struct http_client_t* http_client_create(struct http_transport_t* transport, const char* scheme, const char* ip, unsigned short port)
{
	int r;
	struct http_client_t* http;

	if(!ip || 0 == *ip) return NULL;
	http = (struct http_client_t *)calloc(1, sizeof(*http));
	if(!http) return NULL;

    locker_create(&http->locker);
    http->transport = transport ? transport : http_transport_default();
    http->ref = 1;
    
    http->port = port;
	http->socket = socket_invalid;
    r = snprintf(http->host, sizeof(http->host), "%s", ip);
    if(r > 0 && r < sizeof(http->host))
        r = snprintf(http->scheme, sizeof(http->scheme), "%s", scheme);
    
	http->req = http_request_create(HTTP_1_1);
	http->parser = http_parser_create(HTTP_PARSER_RESPONSE, http_client_onbody, http);
	if (r <= 0 || r >= sizeof(http->scheme) || !http->parser || !http->req)
	{
		http_client_release(http);
		return NULL;
	}
	return http;
}

void http_client_destroy(struct http_client_t* http)
{
	locker_lock(&http->locker);
	http->onreply = NULL; // disable future callback
	locker_unlock(&http->locker);

	http_client_release(http);
}

void http_client_release(struct http_client_t* http)
{
	if (0 == atomic_decrement32(&http->ref))
	{
		if (http->connection)
		{
            http->transport->close(http->connection);
			http->connection = NULL;
		}

		if (http->cookie)
		{
			assert(http->ncookie > 0);
			free(http->cookie);
			http->cookie = NULL;
		}

		if (http->req)
		{
			http_request_destroy(http->req);
			http->req = NULL;
		}

		if (http->parser)
		{
			http_parser_destroy(http->parser);
			http->parser = NULL;
		}
        
        if(http->body.ptr)
        {
            free(http->body.ptr);
            http->body.ptr = NULL;
            http->body.cap = 0;
        }

        http_client_redirect_url_clean(http);
		locker_destroy(&http->locker);
		free(http);
	}
}

void* http_client_getconnection(http_client_t* http)
{
    return http->connection;
}

void http_client_set_timeout(struct http_client_t* http, int conn, int recv, int send)
{
	assert(conn >= 0 && recv >= 0 && send >= 0);
    http->transport->connect_timeout = conn;
    if(http->connection)
	    http->transport->settimeout(http->connection, conn, recv, send);
}

void http_client_set_redirect(http_client_t* http, int (*onredirect)(void* param, const char* urls[], int n), void* param)
{
    http->redirect.onredirect = onredirect;
    http->redirect.param = param;
}

int http_client_get(struct http_client_t* http, const char* uri, const struct http_header_t *headers, size_t n, http_client_onresponse onreply, void* param)
{
    http_client_redirect_url_clean(http);
    http->redirect.n = 1;
    http->redirect.urls[0] = (char*)uri;
 	return http_client_request(http, HTTP_GET, uri, headers, n, NULL, 0, onreply, param);
}

int http_client_post(struct http_client_t* http, const char* uri, const struct http_header_t *headers, size_t n, const void* msg, size_t bytes, http_client_onresponse onreply, void* param)
{
    http_client_redirect_url_clean(http);
    http->redirect.n = 1;
    http->redirect.urls[0] = (char*)uri;
	return http_client_request(http, HTTP_POST, uri, headers, n, msg, bytes, onreply, param);
}

const char* http_client_get_header(struct http_client_t* http, const char *name)
{
	return http_get_header_by_name(http->parser, name);
}

int http_client_read(http_client_t* http, void *data, size_t bytes, int flags, void (*onread)(void* param, int code, void* data, size_t bytes), void* param)
{
    int r, needmore;
    http->body.data = (uint8_t*)data;
    http->body.readed = 0;
    http->body.bytes = (int)bytes;
    http->body.flags = flags;
    http->body.onread = onread;
    http->body.param = param;

    // break Block-IO Depth Callback
    do
    {
        needmore = 0;
        r = http_client_dobody(http, &needmore);
        if(0 != r)
            return r;
        
        if(needmore)
        {
            assert(!http->transport->is_aio);
            r = http->transport->recv(http->connection, http_client_onread_body, http);
        }
        
    } while(0 == r && needmore);
    
    return r;
}
