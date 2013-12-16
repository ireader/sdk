#include "http.h"
#include "list.h"

enum { SM_FIRSTLINE=0, SM_HEADER, SM_BODY };

struct http_status_line
{
	size_t status_code;
	size_t reason_off;
	size_t reason_len; // HTTP reason
};

struct http_request_line
{
	char method[16];
	size_t requri_off;
	size_t requri_len;
};

struct http_header
{
	struct list_head list;
	int name_off, name_len;
	int value_off, value_len;
};

struct http_context
{
	mmptr raw;
	int stateM;
	size_t offset;
	size_t headers; // the number of http header

	size_t verminor, vermajor;
	enum
	{
		struct http_request_line req;
		struct http_status_line reply;
	};

	struct http_header headers[16];
	struct list_head headers2;
	size_t content_length;
};

static size_t s_body_max_size = 2*1024*1024;


// RFC 2612 H2.2
// token = 1*<any CHAR except CTLs or separators>
// separators = "(" | ")" | "<" | ">" | "@"
//				| "," | ";" | ":" | "\" | <">
//				| "/" | "[" | "]" | "?" | "="
//				| "{" | "}" | SP | HT
#define isseparators(c)	((c)=='(' || (c)==')' || (c)=='<' || (c)=='>' || (c)=='@' \
						|| (c)==',' || (c)==';' || (c)==':' || (c)=='\\' || (c)=='"' \ 
						|| (c)=='/' || (c)=='[' || (c)==']' || (c)=='?' || (c)=='=' \
						|| (c)=='{' || (c)=='}' || (c)==' ' || (c)=='\t')

#define isspace(c)		((c)==' ')

//////////////////////////////////////////////////////////////////////////
/// receive
//////////////////////////////////////////////////////////////////////////
static int http_parse_request_line(struct http_context *ctx, const char* line, size_t size)
{
	// H5.1 Request-Line
	// Request-Line = Method SP Request-URI SP HTTP-Version CRLF
	// GET http://www.w3.org/pub/WWW/TheProject.html HTTP/1.1
	const char* p;
	const char* end = line + size;

	// Method
	for(p = line; p < end; ++p)
	{
		if(!isalpha(*p)) break;
	}

	assert(p - line < sizeof(ctx->req.method)-1);
	memmove(ctx->req.method, line, p-line);
	ctx->req.method[p-line] = '\0';

	// SP
	assert(isspace(*p));
	while(p < end && isspace(*p)) ++p;

	// H5.1.2 Request-URI
	// Request-URI = "*" | absoluteURI | abs_path | authority
	while(p < end && (isalnum(*p) || ':'==*p || '/'==*p || '\\'==*p || '+'==*p || '%'==*p))
		++p;

	// SP
	assert(isspace(*p));
	while(p < end && isspace(*p)) ++p;

	// HTTP/1.1
	assert(0 == strnicmp("HTTP/", p, 5));
	if(2 != sscanf(p, "HTTP/%d.%d", &ctx->vermajor, &ctx->verminor))
		return -1; // format error

	assert('\r'==p[5] && '\n'==p[6]);
	return p - line + 8;
}

static int http_parse_status_line(struct http_context *ctx, const char* line, size_t size)
{
	// H6.1 Status-Line
	// Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
	const char* p = line + 8;
	const char* end = line + size;

	// HTTP/1.1
	if(2 != sscanf(line, "HTTP/%1d.%1d",&ctx->vermajor, &ctx->verminor))
		return -1;

	// SP
	assert(isspace(*p));
	while(p < end && isspace(*p)) ++p;

	// Status-Code
	ctx->reply.code = atoi(p);
	assert(ctx->reply.code >= 100 && ctx->reply.code <= 999);

	// SP
	p = p + 3;
	assert(isspace(*p));
	while(p < end && isspace(*p)) ++p;

	// Reason
	ctx->reply.reason_off = p - line;
	while(p < end && *p != '\n') ++p;

	ctx->reply.reason_len = p - line - ctx->reply.reason_off;
	return p - line;
}

inline int is_valid_token(const char* s, int len)
{
	const char *p;
	for(p = s; p < s + len && *p; ++p)
	{
		// CTLs or separators
		if(*p <= 31 || *p >= 127 || isseparators(*p))
			break;
	}

	return p == s+len ? 1 : 0;
}

inline const char* trim_left_right(const char* s, int *n)
{
	const char* p = s + n - 1;

	// left trim
	if('\r' == *s && n > 0) ++s; // some os such as mac use \n\r
	while(s <= p && isspace(*s)) ++s;

	// right trim
	if(p >= s && '\r'==*p) --p;
	while(p >= s && isspace(*p)) --p;

	*n = p + 1 - s;
	return s;
}

static const char* http_parse_line(const char* s, int len, struct http_header *header)
{
	// message-header = field-name ":" [ field-value ]
	// field-name = token
	// field-value = *( field-content | LWS )
	// field-content = <the OCTETs making up the field-value
	//					and consisting of either *TEXT or combinations
	//					of token, separators, and quoted-string>

	const char *name, *value;
	const char *end = s + len;

	// filed name
	for(name = s; s < end && *s; ++s, ++name)
	{
		if(':' == *s || '\n' == *s)
			break;
	}

	if(s >= end)
		return NULL;

	// check ':'
	if(':' != *s)
		return s; // \r\n

	header->name_len = s - name;

	// filed value
	for(value = ++s; s < end && *s; ++s)
	{
		if('\n' == *s)
			break;
	}

	if(s >= end)
		return NULL;

	header->value_len = s - value;

	trim_left_right(name, &header->name_len);
	trim_left_right(value, &header->value_len);

	// filter \"
	if(header->value_len > 0 && '"' == value[0])
	{
		++value;
		--header->value_len;
		if(header->value_len > 0 && '"' == value[header->value_len-1])
			--header->value_len;
	}

	assert(header->name_len > 0 && is_valid_token(name, header->name_len));
	header->name_off = name - s;
	header->value_len = value - s;
	return s;
}

static int http_header_content_length(struct http_context *ctx, const char* value)
{
	ctx->content_length = atoi(value);
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

int http_get_max_size()
{
	return s_body_max_size;
}

int http_set_max_size(size_t bytes)
{
	s_body_max_size = bytes;
	return 0;
}

int http_create()
{
}

int http_clear(void* http)
{
	http_context *ctx = (http_context*)http;
	ctx->content_length = -1;
	ctx->stateM = SM_FIRSTLINE;
	return 0;
}

int http_input(void* parser, const void* data, int bytes)
{
	int r;
	const char *line, *next;
	struct http_header header;
	http_context *ctx = (http_context*)parser;

	if(SM_FIRSTLINE == ctx->stateM)
	{
		assert(0 == ctx->offset);

		// response status
		r = http_parse_request_line(ctx, ctx->raw, ctx->raw.size());
		if(r < 0)
			r = http_parse_status_line(ctx, ctx->raw, ctx->raw.size());

		if(r > 0)
		{
			ctx->offset = r + 2; // \r\n
			ctx->stateM = SM_HEADER;
		}
	}

	if(SM_HEADER == ctx->stateM)
	{
		// parse header
		memset(&header, 0, sizeof(header));
		line = (const char*)ctx->raw + ctx->offset;
		next = http_parse_line(line, ctx->raw.size() - ctx->offset, &header);
		while(next)
		{
			assert('\n'==*next && '\r'==*(next-1));

			// \r\n or \n\r or \n only
			ctx->offset += next + 1 - line;

			if(line == next-1)
			{
				// recv "\r\n\r\n"
				ctx->stateM = SM_BODY;
				break;
			}
			else
			{
				http_parse_header(ctx, &header);
			}

			next = http_parse_line(line, ctx->raw.size() - ctx->offset, &header);
		}
	}

	if(SM_BODY == ctx->stateM)
	{
		if(0 == ctx->content_length)
		{
			return bytes - (ctx->raw.size() - ctx->offset);
		}
		else if(0 < ctx->content_length)
		{
			return bytes - (ctx->raw.size() - ctx->offset - ctx->content_length);
		}
		else
		{
			// don't specify content length, 
			// receive all until socket closed
			return bytes;
		}
	}

	return bytes; // eat all
}
