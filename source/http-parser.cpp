#include "http-parser.h"

//////////////////////////////////////////////////////////////////////////
/// receive
//////////////////////////////////////////////////////////////////////////
static int http_parse_request_line(HttpServer* ctx, const char* s)
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

static int http_parse_status_line(HttpServer* ctx, const char* s)
{
	int majorv, minorv, code;
	char reason[256] = {0};
	if(4 != sscanf(s, "HTTP/%1d.%1d %3d %255s",&majorv, &minorv, &code, reason))
		return -1;

	ctx->path.assign(url);
	ctx->method.assign(method);

	sprintf(method, "%d.%d", majorv, minorv);
	ctx->version.assign(method);
	return 0;
}

#define isspace(c) (c==' ')
#define isseparators(c)	(c=='(' || c==')' || c=='<' || c=='>' || c=='@' \
						|| c==',' || c==';' || c==':' || c=='\\' || c=='"' \ 
						|| c=='/' || c=='[' || c==']' || c=='?' || c=='=' \
						|| c=='{' || c=='}' || c==' ' || c=='\t')

// RFC 2612 H2.2
// token = 1*<any CHAR except CTLs or separators>
// separators = "(" | ")" | "<" | ">" | "@"
//				| "," | ";" | ":" | "\" | <">
//				| "/" | "[" | "]" | "?" | "="
//				| "{" | "}" | SP | HT
static int http_parse_name_value(char* s, int len, char*& name, char*& value)
{
	// parse response header name value
	// name: value

	int namelen = 0;
	int valuelen = 0;

	// filter whitespace
	while(*s && isspace(*s)) ++s;

	for(name=s; *s && *s > 31 && *s < 127 && !isseparators(c); ++s)
		++namelen;

	while(*s && ':' != *s) ++s;

	if(':' != *s)
		return -1;

	++s; // skip ':'

	while(*s && isspace(*s)) ++s; // filter SP

	for(value = s; *s && '\r' != *s && '\n' != *s; ++s)
		++valuelen;

	// filter SP
	for(--s; s>value && isspace(*s); --s)
		--valuelen;

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

struct http_context
{
	mmptr raw;
	size_t offset;
	size_t hdsize; // head size
	size_t maxsize;
}

int http_parser_create()
{

}

int http_parser_input(const void* data, int bytes)
{
	int r = 0;
	int lineNo = 0;	// line number
	http_context *ctx = (http_context*)p;

	// raw data
	if(ctx->raw.size() + bytes > ctx->maxsize)
		return -1;

	ctx->raw.append(data, bytes);

	// parse header
	const char* line = (const char*)ctx->raw + ctx->offset;
	for(char* p = strchr(line, '\n'); p; p = strchr(line, '\n'))
	{
		// \r\n or \n\r or \n only
		const p2 = (p>line && *(p-1)=='\r') ? p-1 : (p[1]=='\r' ? p+1 : p);
		ctx->offset += max(p2, p)+1 - line; // next line

		if(0 == ctx->offset)
		{
			// response status
			if(0 == http_parse_request_line(ctx, line))
				return 0;

			if(0 == http_parse_status_line(ctx, line))
				return ERROR_REPLY;
		}
		else
		{
			if(line+ctx->offset == min(p, p2))
			{
				// recv "\r\n\r\n"

				// head size
				ctx->hdsize = ctx->offset + max(p, p2)+1-line;

				http_recv_content(ctx);
			}
			else
			{
				http_parse_header(ctx, line, p-line);
			}
		}		
	}
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
