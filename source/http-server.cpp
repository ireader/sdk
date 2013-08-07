#include "http-server.h"
#include "cstringext.h"
#include "mmptr.h"
#include "error.h"
#include <map>
#include <string>

struct less
{
	bool operator()(const std::string& l, const std::string& r) const
	{
		return stricmp(l.c_str(), r.c_str())<0;
	}
};
typedef std::map<std::string, std::string, less> HttpHeaders;

typedef struct _http_server_context
{
	socket_t socket;
	int recvTimeout;
	int sendTimeout;
	std::string reply;

	std::string path;
	std::string method;
	std::string version;
	HttpHeaders headers;
	size_t contentLength;
	int connection;

	mmptr postdata;
	mmptr ptr;
} HttpServer;

void* http_server_create(socket_t sock)
{
	HttpServer* ctx;
	ctx = new HttpServer;
	ctx->socket = sock;
	return ctx;
}

int http_server_destroy(void **server)
{
	HttpServer* ctx;
	if(server && *server)
	{
		ctx = (HttpServer*)*server;
		if(ctx->socket != socket_invalid)
			socket_close(ctx->socket);
		delete ctx;
		*server = 0;
	}
	return 0;
}

void http_server_set_timeout(void *server, int recv, int send)
{
	HttpServer* ctx = (HttpServer*)server;
	ctx->recvTimeout = recv;
	ctx->sendTimeout = send;
}

void http_server_get_timeout(void *server, int *recv, int *send)
{
	HttpServer* ctx = (HttpServer*)server;
	*recv = ctx->recvTimeout;
	*send = ctx->sendTimeout;
}

const char* http_server_get_path(void *server)
{
	HttpServer* ctx = (HttpServer*)server;
	return ctx->path.c_str();
}

const char* http_server_get_method(void *server)
{
	HttpServer* ctx = (HttpServer*)server;
	return ctx->method.c_str();
}

int http_server_get_content(void *server, void **content, int *length)
{
	HttpServer* ctx = (HttpServer*)server;
	*content = ctx->postdata.get();
	*length = ctx->postdata.size();
	return 0;
}

const char* http_server_get_header(void *server, const char *name)
{
	HttpHeaders& headers = ((HttpServer*)server)->headers;
	HttpHeaders::iterator it;
	it = headers.find(name);
	if(it == headers.end())
		return NULL;
	return it->second.c_str();
}

//////////////////////////////////////////////////////////////////////////
/// receive
//////////////////////////////////////////////////////////////////////////
static int http_parse_name_value(char* s, int len, char*& name, char*& value)
{
	// parse response header name value
	// name: value
	char* p = strchr(s, ':');
	if(NULL == p)
	{
		assert(false);
		return -1;
	}

	name = s;
	value = p+1; // skip ':'

	while(' ' == *name) ++name; // trim left space
	do{ --p; } while(p>name && ' '==*p); // skip ':' and trim right space
	*++p = 0;

	while(' ' == *value) ++value; // trim left space
	p = s+len;
	do{ --p; } while(p>value && (' '==*p||'\r'==*p)); // trim right space
	*++p = 0;

	return 0 == *name ? -1 : 0; // name can't be empty
}

static int http_parse_firstline(HttpServer* ctx, const char* s)
{
	char url[256] = {0};
	char method[16] = {0};
	int majorv, minorv;
	if(4 != sscanf(s, "%15s %255s HTTP/%d.%d", method, url, &majorv, &minorv))
		return -1;

	ctx->path.assign(url);
	ctx->method.assign(method);

	sprintf(method, "%d.%d", majorv, minorv);
	ctx->version.assign(method);
	return 0;
}

static int http_parse_header(HttpServer* ctx, char* reply, size_t len)
{
	char* name;
	char* value;
	assert(len > 2 && reply[len]=='\n' && reply[len-1]=='\r');

	// *--p = 0; // \r -> 0
	if(0 != http_parse_name_value(reply, len, name, value))
		return -1;

	if(0 == stricmp("Content-Length", name))
	{
		// < 50 M
		int contentLen = atoi(value);
		assert(contentLen >= 0 && contentLen < 50*1024*1024);
		ctx->contentLength = contentLen;
	}
	else if(0 == stricmp("Connection", name))
	{
		ctx->connection = strieq("close", value) ? 1 : 0;
	}

	//response.SetHeader(name, value);
	ctx->headers.insert(std::make_pair(name, value));
	return 0;
}

static int http_recv_line(mmptr& reply, socket_t socket, int timeout)
{
	const size_t c_length = 20; // content minimum length
	size_t n = reply.size();
	char* p;

	do
	{
		if(reply.capacity()<n+c_length)
		{
			if(reply.reserve(n+c_length))
				return ERROR_MEMORY;
		}

		p = (char*)reply.get() + n;
		int r = socket_recv_by_time(socket, p, c_length-1, 0, timeout);
		if(r <= 0)
			return r;

		n += r;
		p[r] = 0;
	} while(!strchr(p, '\n') && n<20*1024*1024);

	assert(reply.capacity()>n);
	return n;
}

static int http_recv_header(HttpServer* ctx)
{
	int r = 0;
	int lineNo = 0;	// line number
	do
	{
		// recv header
		r = http_recv_line(ctx->ptr, ctx->socket, ctx->recvTimeout);
		if(r <= 0)
			return ERROR_RECV_TIMEOUT;

		// parse header
		char* line = ctx->ptr;
		char* p = strchr(line, '\n');
		assert(p);
		while(p)
		{
			assert(p>line && *(p-1)=='\r');
			if(0 == lineNo)
			{
				// response status
				if(0!=http_parse_firstline(ctx, line))
					return ERROR_REPLY;
			}
			else
			{
				if(p-1 == line)
				{
					// recv "\r\n\r\n"
					// copy content data(don't copy with '\0')
					ctx->ptr.set(p+1, r-(p+1-(char*)(ctx->ptr)));
					return 0;
				}

				http_parse_header(ctx, line, p-line);
			}

			++lineNo;
			line = p+1;
			p = strchr(line, '\n');
		}

		// copy remain data
		ctx->ptr.set(line, r-(line-(char*)(ctx->ptr)));
	} while(r > 0);

	return 0;
}

int http_recv_content(HttpServer* ctx)
{
	assert(ctx->contentLength>0 && ctx->contentLength<1024*1024*1024);
	if(ctx->postdata.capacity() < ctx->contentLength)
	{
		if(0 != ctx->postdata.reserve(ctx->contentLength))
			return ERROR_MEMORY;
	}

	// copy receive data
	ctx->postdata.set(ctx->ptr.get(), ctx->ptr.size());

	// receive content
	assert(ctx->contentLength >= ctx->ptr.size());
	int n = (int)(ctx->contentLength-ctx->ptr.size());
	if(n > 0)
	{
		void* p = (char*)ctx->postdata.get() + ctx->postdata.size();
		int r = socket_recv_all_by_time(ctx->socket, p, n, 0, ctx->recvTimeout);
		if(0 == r)
			return ERROR_RECV_TIMEOUT; // timeout
		else if(r < 0)
			return ERROR_RECV;
		assert(r == n);
	}

	ctx->postdata.set(ctx->postdata.get(), ctx->contentLength); // set request size
	ctx->postdata.append('\0');
	return 0;
}

int http_server_recv(void *server)
{
	int r;
	HttpServer* ctx = (HttpServer*)server;
	ctx->headers.clear();
	ctx->version.clear();
	ctx->method.clear();
	ctx->path.clear();
	ctx->ptr.clear();
	ctx->postdata.clear();
	ctx->reply.clear();
	ctx->contentLength = 0;

	r = http_recv_header(ctx);
	if(0==r && ctx->contentLength > 0)
		r = http_recv_content(ctx);
	return r;
}

int http_server_send(void* server, int code, const void* data, int bytes)
{
	char status[256] = {0};
	socket_bufvec_t vec[4];
	HttpServer* ctx = (HttpServer*)server;

	sprintf(status, "HTTP/1.1 %d OK\r\n", code);

	socket_setbufvec(vec, 0, status, strlen(status));
	socket_setbufvec(vec, 1, (void*)ctx->reply.c_str(), ctx->reply.length());
	socket_setbufvec(vec, 2, (void*)"\r\n", 2);
	socket_setbufvec(vec, 3, (void*)data, bytes);

	int r = socket_send_v(ctx->socket, vec, bytes>0?4:3, 0);
	return (r==(int)(strlen(status)+ctx->reply.length()+2+bytes))?0:-1;
}

int http_server_set_header(void *server, const char* name, const char* value)
{
	char msg[256] = {0};
	HttpServer* ctx = (HttpServer*)server;
	sprintf(msg, "%s: %s\r\n", name, value);
	ctx->reply += msg;
	return 0;
}

int http_server_set_header_int(void *server, const char* name, int value)
{
	char msg[256] = {0};
	HttpServer* ctx = (HttpServer*)server;
	sprintf(msg, "%s: %d\r\n", name, value);
	ctx->reply += msg;
	return 0;
}
