#include "http.h"
#include "cstringext.h"
#include "list.h"
#include <stdio.h>
#include <stdlib.h>

#define KB (1024)
#define MB (1024*1024)

enum { SM_FIRSTLINE = 0, SM_HEADER = 100, SM_BODY = 200 };

struct http_status_line
{
	int code;
	size_t reason_pos;
	size_t reason_len; // HTTP reason
};

struct http_request_line
{
	char method[16];
	size_t uri_pos;
	size_t uri_len;
};

struct http_header
{
	struct list_head list;
	size_t npos, nlen; // name
	size_t vpos, vlen; // value
};

struct http_context
{
	char *raw;
	size_t raw_size;
	size_t raw_capacity;
	int server_mode; // 0-client, 1-server
	int stateM;
	size_t offset;
	
	// start line
	int verminor, vermajor;
	union
	{
		struct http_request_line req;
		struct http_status_line reply;
	};

	// headers
	struct http_header headers[16];
	struct list_head headers2;
	size_t headers; // the number of http header
	size_t content_length;
};

static size_t s_body_max_size = 2*MB;


// RFC 2612 H2.2
// token = 1*<any CHAR except CTLs or separators>
// separators = "(" | ")" | "<" | ">" | "@"
//				| "," | ";" | ":" | "\" | <">
//				| "/" | "[" | "]" | "?" | "="
//				| "{" | "}" | SP | HT
#define isseparators(c)	(!!strchr("()<>@,;:\\\"/[]?={} \t", (c)))
#define isspace(c)		((c)==' ')

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

inline void trim_left_right(const char* s, size_t *pos, size_t *len)
{
	// left trim
	while(*len > 0 && isspace(s[*pos]))
	{
		--*len;
		++*pos;
	}

	// right trim
	while(*len > 0 && isspace(s[*pos + *len - 1]))
	{
		--*len;
	}
}

static int http_rawdata(struct http_context *ctx, const void* data, int bytes)
{
	void *p;
	int capacity;

	if(ctx->raw_capacity - ctx->raw_size < (size_t)bytes)
	{
		capacity = (ctx->raw_capacity > 4*MB) ? 50*MB : (ctx->raw_capacity > 16*KB ? 2*MB : 8*KB);
		p = realloc(ctx->raw, ctx->raw_capacity + max(bytes, capacity));
		if(!p)
			return ENOMEM;

		memmove(p, ctx->raw, ctx->raw_size);
		ctx->raw_capacity += max(bytes, capacity);
		ctx->raw = p;
	}

	assert(ctx->raw_capacity - ctx->raw_size > (size_t)bytes);
	memmove((char*)ctx->raw + ctx->raw_size, data, bytes);
	ctx->raw_size += bytes;
	return 0;
}

static int http_parse_request_line(struct http_context *ctx)
{
	// H5.1 Request-Line
	// Request-Line = Method SP Request-URI SP HTTP-Version CRLF
	// GET http://www.w3.org/pub/WWW/TheProject.html HTTP/1.1

	enum { SM_REQUEST_METHOD=SM_FIRSTLINE, SM_REQUEST_URI, SM_REQUEST_VERSION, SM_REQUEST_END };

	for(; ctx->offset < ctx->raw_size; ctx->offset++)
	{
		switch(ctx->stateM)
		{
		case SM_REQUEST_METHOD:
			assert('\r' != ctx->raw[ctx->offset]);
			assert('\n' != ctx->raw[ctx->offset]);
			if(' ' == ctx->raw[ctx->offset])
			{
				assert(ctx->offset < sizeof(ctx->req.method)-1);
				strncpy(ctx->req.method, ctx->raw, ctx->offset);
				ctx->stateM = SM_REQUEST_URI;
				assert(0 == ctx->req.uri_pos);
				assert(0 == ctx->req.uri_len);
			}
			break;

			// H5.1.2 Request-URI
			// Request-URI = "*" | absoluteURI | abs_path | authority
		case SM_REQUEST_URI:
			assert('\r' != ctx->raw[ctx->offset]);
			assert('\n' != ctx->raw[ctx->offset]);
			if(' ' == ctx->raw[ctx->offset])
			{
				if(0 == ctx->req.uri_pos)
				{
					break; // skip SP
				}
				else
				{
					assert(0 == ctx->req.uri_len);
					ctx->req.uri_len = ctx->offset - 1 - ctx->req.uri_pos;
					ctx->stateM = SM_REQUEST_VERSION;
				}
			}
			else
			{
				// validate uri
				assert(isalnum(ctx->raw[ctx->offset]) || strchr(".:?/\\+%", ctx->raw[ctx->offset]));
				if(0 == ctx->req.uri_pos)
					ctx->req.uri_pos = ctx->offset;
			}
			break;

		case SM_REQUEST_VERSION:
			assert('\r' != ctx->raw[ctx->offset]);
			assert('\n' != ctx->raw[ctx->offset]);
			if(ctx->offset + 8 > ctx->raw_size)
				return 0; // wait for more data

			if(' ' == ctx->raw[ctx->offset])
				break; // skip SP

			// HTTP/1.1
			if(2 != sscanf(ctx->raw+ctx->offset, "HTTP/%1d.%1d",&ctx->vermajor, &ctx->verminor))
				return -1;

			assert(1 == ctx->vermajor);
			assert(1 == ctx->verminor || 0 == ctx->verminor);
			ctx->offset += 7; // skip
			ctx->stateM = SM_REQUEST_END;
			break;

		case SM_REQUEST_END:
			switch(ctx->raw[ctx->offset])
			{
			case ' ':
			case '\r':
				break;
			case '\n':
				ctx->stateM = SM_HEADER;
				break;
			default:
				assert(0);
				return -1; // invalid
			}
			break;

		default:
			assert(0);
			return -1;
		}
	}

	return 0;
}

static int http_parse_status_line(struct http_context *ctx)
{
	// H6.1 Status-Line
	// Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF

	int i;
	enum { SM_STATUS_VERSION=SM_FIRSTLINE, SM_STATUS_CODE, SM_STATUS_REASON };

	for(; ctx->offset < ctx->raw_size; ctx->offset++)
	{
		switch(ctx->stateM)
		{
		case SM_STATUS_VERSION:
			assert('\r' != ctx->raw[ctx->offset]);
			assert('\n' != ctx->raw[ctx->offset]);
			if(ctx->offset + 8 > ctx->raw_size)
				return 0; // wait for more data

			assert(0 == ctx->offset);
			if(2 != sscanf(ctx->raw+ctx->offset, "HTTP/%1d.%1d",&ctx->vermajor, &ctx->verminor))
				return -1;

			assert(1 == ctx->vermajor);
			assert(1 == ctx->verminor || 0 == ctx->verminor);
			ctx->offset += 7; // skip
			ctx->stateM = SM_STATUS_CODE;
			break;

		case SM_STATUS_CODE:
			assert('\r' != ctx->raw[ctx->offset]);
			assert('\n' != ctx->raw[ctx->offset]);
			if(' ' == ctx->raw[ctx->offset])
				break; // skip SP

			if('0' > ctx->raw[ctx->offset] || ctx->raw[ctx->offset] > '9')
				return -1; // invalid

			if(ctx->offset + 3 > ctx->raw_size)
				return 0; // wait for more data

			assert(0 == ctx->reply.code);
			for(i = 0; i < 3; i++)
				ctx->reply.code = ctx->reply.code * 10 + (ctx->raw[ctx->offset+i] - '0');

			ctx->offset += 2; // skip
			ctx->stateM = SM_STATUS_REASON;
			ctx->reply.reason_pos = ctx->offset+1;
			assert(isspace(ctx->raw[ctx->reply.reason_pos]));
			break;

		case SM_STATUS_REASON:
			switch(ctx->raw[ctx->offset])
			{
			//case '\r':
			//	break;
			case '\n':
				assert('\r' == ctx->raw[ctx->offset-1]);
				ctx->reply.reason_len = ctx->offset - 2 - ctx->reply.reason_pos;
				trim_left_right(ctx->raw, &ctx->reply.reason_pos, &ctx->reply.reason_len);
				ctx->stateM = SM_HEADER;
				break;

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

static int http_parse_header_line(struct http_context *ctx, struct http_header *header)
{
	// H4.2 Message Headers
	// message-header = field-name ":" [ field-value ]
	// field-name = token
	// field-value = *( field-content | LWS )
	// field-content = <the OCTETs making up the field-value
	//					and consisting of either *TEXT or combinations
	//					of token, separators, and quoted-string>

	enum { SM_HEADER_START=SM_HEADER, SM_HEADER_NAME, SM_HEADER_VALUE };
	for(; ctx->offset < ctx->raw_size; ctx->offset++)
	{
		switch(ctx->stateM)
		{
		case SM_HEADER_START:
			switch(ctx->raw[ctx->offset])
			{
			case '\r':
				if(ctx->offset + 2 > ctx->raw_size)
					return 0; // wait more date

				++ctx->offset;
				assert('\n' == ctx->raw[ctx->offset]);

			case '\n':
				ctx->stateM = SM_BODY;
				return 0;

			case ' ':
			case '\t':
				assert(0); // multi-line header ?
				break;

			default:
				assert(0 == header->npos);
				assert(0 == header->nlen);
				header->npos = ctx->offset;
				ctx->stateM = SM_HEADER_NAME;
			}

		case SM_HEADER_NAME:
			assert('\r' != ctx->raw[ctx->offset]);
			assert('\n' != ctx->raw[ctx->offset]);
			switch(ctx->raw[ctx->offset])
			{
			case '\r':
			case '\n':
				return -1; // invalid

			case ':':
				header->nlen = ctx->offset - 1 - header->npos;
				trim_left_right(ctx->raw+header->npos, &header->npos, &header->nlen);
				assert(header->nlen > 0 && is_valid_token(ctx->raw+header->npos, header->nlen));
				ctx->stateM = SM_HEADER_VALUE;
				header->vpos = ctx->offset + 1;
				break;
			}
			break;

		case SM_HEADER_VALUE:
			switch(ctx->raw[ctx->offset])
			{
			case '\n':
				assert('\r' == ctx->raw[ctx->offset-1]);
				header->vlen = ctx->offset - 2 - header->vpos;
				trim_left_right(ctx->raw, &header->vpos, &header->vlen);
				ctx->stateM = SM_HEADER;
				break;

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

static int http_header_content_length(struct http_context *ctx, const char* value)
{
	ctx->content_length = atoi(value);
	return 0;
}

static int http_parse_header(struct http_context *ctx)
{
	//if(0 == stricmp("Content-Length", name))
	//{
	//	// < 50 M
	//	int contentLen = atoi(value);
	//	assert(contentLen >= 0 && contentLen < 50*1024*1024);
	//	ctx->contentLength = contentLen;
	//}
	//else if(0 == stricmp("Connection", name))
	//{
	//	ctx->connection = strieq("close", value) ? 1 : 0;
	//}

	////response.SetHeader(name, value);
	//ctx->headers.insert(std::make_pair(name, value));
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

void* http_create(int mode)
{
	struct http_context *ctx;
	ctx = (struct http_context*)malloc(sizeof(struct http_context));
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(struct http_context));
	ctx->server_mode = mode;
	http_clear();
	return ctx;
}

int http_destroy(void* http)
{
	struct http_context *ctx;
	ctx = (struct http_context*)http;
	if(ctx->raw)
	{
		assert(ctx->raw_capacity > 0);
		free(ctx->raw);
		ctx->raw = 0;
		ctx->raw_size = 0;
		ctx->raw_capacity = 0;
	}
	return 0;
}

int http_clear(void* http)
{
	struct http_context *ctx;
	ctx = (struct http_context*)http;
	memset(&ctx->req, 0, sizeof(ctx->req));
	memset(&ctx->reply, 0, sizeof(ctx->reply));
	ctx->stateM = SM_FIRSTLINE;
	ctx->content_length = -1;
	ctx->headers = 0;
	return 0;
}

int http_input(void* parser, const void* data, int bytes)
{
	int i, r;
	const char *line, *next;
	struct http_header header;
	struct http_context *ctx;

	// save raw data
	ctx = (struct http_context*)parser;
	r = http_rawdata(ctx, data, bytes);
	if(0 != r)
	{
		assert(r < 0);
		return r;
	}

	if(SM_FIRSTLINE <= ctx->stateM && ctx->stateM < SM_HEADER)
	{
		r = ctx->server_mode ? http_parse_request_line(ctx) : http_parse_status_line(ctx);
	}

	if(SM_HEADER <= ctx->stateM && ctx->stateM < SM_BODY)
	{
		r = http_parse_header_line(ctx);
	}

	assert(r <= 0);
	if(SM_BODY == ctx->stateM)
	{
		if(0 == ctx->content_length)
		{
			return bytes - (ctx->raw_size - ctx->offset);
		}
		else if(0 < ctx->content_length)
		{
			return bytes - (ctx->raw_size - ctx->offset - ctx->content_length);
		}
		else
		{
			// don't specify content length, 
			// receive all until socket closed
			return bytes;
		}
	}

	return 0 == r ? bytes : r; // eat all
}
