#include "http-parser.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64) || defined(OS_WINDOWS)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#define KB (1024)
#define MB (1024*1024)
#define HTTP_HEADER_LENGTH_MAX	(2*MB)
#define HTTP_BODY_LENGTH_MAX	(64*MB)

enum { SM_START_LINE = 0, SM_HEADER = 100, SM_BODY = 200, SM_DONE = 300 };

struct http_string_t
{
	size_t pos; // offset from raw data
	size_t len;
};

struct http_status_line_t
{
	int code;
	struct http_string_t reason;
};

struct http_request_line_t
{
	struct http_string_t method;
	struct http_string_t uri;
};

struct http_header_t
{
	struct http_string_t name;
	struct http_string_t value;
};

struct http_chunk_t
{
	size_t len;
	size_t loaded;
};

struct http_parser_t
{
	char *raw;
	size_t raw_size;
	size_t raw_capacity;
	size_t raw_header_offset;
	int stateM;	
	int request; // 1-request, 0-response

	struct http_header_t header;
	struct http_chunk_t chunk;

	// start line
	char protocol[17]; // HTTP/RTSP/SIP
	int verminor, vermajor;
	union
	{
		struct http_request_line_t req;
		struct http_status_line_t reply;
	} u;

	// headers
	struct http_header_t *headers;
	int header_size; // the number of HTTP header
	int header_capacity;
	int64_t content_length; // -1-don't have header, >=0-Content-Length
	int connection_close; // 1-close, 0-keep-alive, <0-don't set
	int content_encoding;
	int transfer_encoding;
	int cookie;
	int location;

	void (*callback)(void* param, const void* data, int len);
	void* param;
	int64_t raw_body_length; // include previous callback data
};

static size_t s_body_max_size = 0*MB;


// RFC 2612 H2.2
// token = 1*<any CHAR except CTLs or separators>
// separators = "(" | ")" | "<" | ">" | "@"
//				| "," | ";" | ":" | "\" | <">
//				| "/" | "[" | "]" | "?" | "="
//				| "{" | "}" | SP | HT
static int is_valid_token(const char* s, size_t len)
{
	const char *p;
	for(p = s; p < s + len && *p; ++p)
	{
		// CTLs or separators
		if(*p <= 31 || *p >= 127 || !!strchr("()<>@,;:\\\"/[]?={} \t", *p))
			break;
	}

	return p == s+len ? 1 : 0;
}

static int is_transfer_encoding_chunked(const struct http_parser_t *http)
{
	return (http->transfer_encoding > 0 && 0==strcasecmp("chunked", http->raw + http->transfer_encoding)) ? 1 : 0;
}

static int http_rawdata(struct http_parser_t *http, const void* data, size_t bytes)
{
	void *p;
	size_t capacity;
	if(http->raw_capacity - http->raw_size < bytes + 1)
	{
		if(http->raw_size + bytes > HTTP_BODY_LENGTH_MAX + http->raw_header_offset)
			return E2BIG;
		capacity = (http->raw_capacity > 4*MB) ? 50*MB : (http->raw_capacity > 16*KB ? 2*MB : 8*KB);
		capacity = (bytes + 1) > capacity ? (bytes + 1) : capacity;
		p = realloc(http->raw, http->raw_capacity + capacity);
		if(!p)
			return ENOMEM;

		http->raw_capacity += capacity;
		http->raw = p;
	}

	assert(http->raw_capacity - http->raw_size > bytes);
	memmove((char*)http->raw + http->raw_size, data, bytes);
	http->raw_size += bytes;
	http->raw[http->raw_size] = '\0'; // auto add ending '\0'
	return 0;
}

static int http_body(struct http_parser_t* http, const void* data, size_t bytes)
{
	if (http->callback)
	{
		http->raw_body_length += bytes;
		http->callback(http->param, data, (int)bytes);
	}
	else
	{
		// save raw data
		if (0 != http_rawdata(http, data, bytes))
		{
			assert(0);
			return -1;
		}

		// saved body
		http->raw_body_length += bytes;
		assert(http->raw_body_length + http->raw_header_offset <= http->raw_size);
		assert(-1 == http->content_length || http->raw_body_length <= http->content_length);
	}

	return 0;
}

// general-header = Cache-Control ; Section 14.9
//					| Connection ; Section 14.10
//					| Date ; Section 14.18
//					| Pragma ; Section 14.32
//					| Trailer ; Section 14.40
//					| Transfer-Encoding ; Section 14.41
//					| Upgrade ; Section 14.42
//					| Via ; Section 14.45
//					| Warning ; Section 14.46
//
// request-header = Accept ; Section 14.1
//					| Accept-Charset ; Section 14.2
//					| Accept-Encoding ; Section 14.3
//					| Accept-Language ; Section 14.4
//					| Authorization ; Section 14.8
//					| Expect ; Section 14.20
//					| From ; Section 14.22
//					| Host ; Section 14.23
//					| If-Match ; Section 14.24
//					| If-Modified-Since ; Section 14.25
//					| If-None-Match ; Section 14.26
//					| If-Range ; Section 14.27
//					| If-Unmodified-Since ; Section 14.28
//					| Max-Forwards ; Section 14.31
//					| Proxy-Authorization ; Section 14.34
//					| Range ; Section 14.35
//					| Referer ; Section 14.36
//					| TE ; Section 14.39
//					| User-Agent ; Section 14.43
//
// response-header = Accept-Ranges ; Section 14.5
//					| Age ; Section 14.6
//					| ETag ; Section 14.19
//					| Location ; Section 14.30
//					| Proxy-Authenticate ; Section 14.33
//					| Retry-After ; Section 14.37
//					| Server ; Section 14.38
//					| Vary ; Section 14.44
//					| WWW-Authenticate ; Section 14.47
//
// entity-header = Allow ; Section 14.7
//					| Content-Encoding ; Section 14.11
//					| Content-Language ; Section 14.12
//					| Content-Length ; Section 14.13
//					| Content-Location ; Section 14.14
//					| Content-MD5 ; Section 14.15
//					| Content-Range ; Section 14.16
//					| Content-Type ; Section 14.17
//					| Expires ; Section 14.21
//					| Last-Modified ; Section 14.29
//					| extension-header
//
// extension-header = message-header
static int http_header_handler(struct http_parser_t *http, size_t npos, size_t vpos)
{
	// TODO: 
	// RFC-2616 4.2 Message Headers p22
	// Multiple message-header fields with the same field-name MAY be present in a message
	const char* name = http->raw + npos;
	const char* value = http->raw + vpos;

	if(0 == strcasecmp("Content-Length", name))
	{
		// H4.4 Message Length, section 3, ignore content-length if in chunked mode
		if(is_transfer_encoding_chunked(http))
			http->content_length = -1;
		else
			http->content_length = strtoll(value, NULL, 10);
		assert(http->content_length >= 0 && (0==s_body_max_size || http->content_length < (int64_t)s_body_max_size));
	}
	else if(0 == strcasecmp("Connection", name))
	{
		http->connection_close = (0==strcasecmp("close", value)) ? 1 : 0;
	}
	else if(0 == strcasecmp("Content-Encoding", name))
	{
		// gzip/compress/deflate/identity(default)
		http->content_encoding = (int)vpos;
	}
	else if(0 == strcasecmp("Transfer-Encoding", name))
	{
		http->transfer_encoding = (int)vpos;
		if(0 == strncasecmp("chunked", value, 7))
		{
			// chunked can't use with content-length
			// H4.4 Message Length, section 3,
			assert(-1 == http->content_length);
			http->raw[http->transfer_encoding + 7] = '\0'; // ignore parameters
		}
	}
	else if(0 == strcasecmp("Set-Cookie", name))
	{
		// TODO: Multiple Set-Cookie headers
		http->cookie = (int)vpos;
	}
	else if(0 == strcasecmp("Location", name))
	{
		http->location = (int)vpos;
	}

	return 0;
}

static int http_header_add(struct http_parser_t *http, struct http_header_t* header)
{
	int size;
	struct http_header_t *p;
	if(http->header_size+1 >= http->header_capacity)
	{
		size = http->header_capacity < 16 ? 16 : (http->header_size * 3 / 2);
		p = (struct http_header_t*)realloc(http->headers, sizeof(struct http_header_t) * size);
		if(!p)
			return ENOMEM;

		http->headers = p;
		http->header_capacity = size;
	}

	assert(header->name.pos > 0);
	assert(header->name.len > 0);
	assert(header->value.pos > 0);
	assert(is_valid_token(http->raw + header->name.pos, header->name.len));
	http->raw[header->name.pos + header->name.len] = '\0';
	http->raw[header->value.pos + header->value.len] = '\0';
	memcpy(http->headers + http->header_size, header, sizeof(struct http_header_t));
	++http->header_size;
	return 0;
}

// H5.1 Request-Line
// Request-Line = Method SP Request-URI SP HTTP-Version CRLF
// GET http://www.w3.org/pub/WWW/TheProject.html HTTP/1.1
//
// RFC2326 (p14)
// 3.2 RTSP URL
//
// RFC3986 
// 2.2 Reserved Characters
// reserved    = gen-delims / sub-delims
// gen-delims  = ":" / "/" / "?" / "#" / "[" / "]" / "@"
// sub-delims  = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
// 2.3.  Unreserved Characters
// unreserved  = ALPHA / DIGIT / "-" / "." / "_" / "~"
//assert(isalnum(http->raw[http->offset]) || strchr("-._~:/?#[]@!$&'()*+,;=", http->raw[http->offset]) || '%' == http->raw[http->offset]);
static int http_parse_request_line(struct http_parser_t *http, const char* data, size_t len)
{
	enum { 
		SM_REQUEST_START = SM_START_LINE, 
		SM_REQUEST_METHOD,
		SM_REQUEST_SP1,
		SM_REQUEST_URI,
		SM_REQUEST_SP2,
		SM_REQUEST_VERSION, 
		SM_REQUEST_SP3,
		SM_REQUEST_CR,
		SM_REQUEST_LF,
	};

	char c;
	size_t i;
	size_t pos;
	size_t *v[6];
	v[0] = &http->u.req.method.pos;
	v[1] = &http->u.req.method.len;
	v[2] = &http->u.req.uri.pos;
	v[3] = &http->u.req.uri.len;
	v[4] = &http->header.name.pos; // version
	v[5] = &http->header.name.len;

	pos = http->raw_size;
	for (i = 0; i < len; i++)
	{
		c = data[i];
		switch(http->stateM)
		{
		case SM_REQUEST_START:
		case SM_REQUEST_SP1:
		case SM_REQUEST_SP2:
			if (' ' != c && '\t' != c)
			{
				*(v[http->stateM - SM_REQUEST_START]) = pos + i;
				http->stateM += 1; // next state
				i -= 1; // go back
			}
			break;

		case SM_REQUEST_METHOD:
		case SM_REQUEST_URI:
		case SM_REQUEST_VERSION:
			if (' ' == c || '\t' == c || '\r' == c || '\n' == c)
			{
				*(v[http->stateM - SM_REQUEST_START]) = pos + i - *(v[http->stateM - SM_REQUEST_START - 1]);
				http->stateM += 1; // next state
				i -= 1; // go back
			}
			break;

		case SM_REQUEST_SP3:
			switch (c)
			{
			case ' ':
			case '\t':
				break; // continue

			case '\r':
				http->stateM = SM_REQUEST_CR;
				break;

			case '\n':
				http->stateM = SM_REQUEST_LF;
				i -= 1; // go back
				break;

			default:
				http->stateM = SM_REQUEST_URI;
				break;
			}
			break;

		case SM_REQUEST_CR:
			if ('\n' != c)
			{
				assert(0);
				return -1;
			}
			http->stateM = SM_REQUEST_LF;
			i -= 1; // go back
			break;

		case SM_REQUEST_LF:
			assert('\n' == c);

			// copy request line
			if (0 != http_rawdata(http, data, i))
				return -ENOMEM;

			// H5.1.1 Method (p24)
			// Method = OPTIONS | GET | HEAD | POST | PUT | DELETE | TRACE | CONNECT | extension-method
			if (http->u.req.method.len < 1
				// H5.1.2 Request-URI (p24)
				// Request-URI = "*" | absoluteURI | abs_path | authority
				|| http->u.req.uri.len < 1
				// H3.1 HTTP Version (p13)
				// HTTP-Version = "HTTP" "/" 1*DIGIT "." 1*DIGIT
				|| http->header.name.len < 5
				|| 3 != sscanf(http->raw + http->header.name.pos, "%16[^/]/%d.%d", http->protocol, &http->vermajor, &http->verminor))
			{
				assert(0);
				return -1;
			}

			http->raw[http->u.req.method.pos + http->u.req.method.len] = '\0';
			http->raw[http->u.req.uri.pos + http->u.req.uri.len] = '\0';
			//assert(1 == http->vermajor || 2 == http->vermajor);
			//assert(1 == http->verminor || 0 == http->verminor);
			http->stateM = SM_HEADER;
			i += 1; // skip '\n'
			return (int)i;

		default:
			assert(0);
			return -1; // invalid
		}
	}

	// copy request line
	if (0 != http_rawdata(http, data, i))
		return -ENOMEM;
	return (int)i; // wait for more data
}

// H6.1 Status-Line
// Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
static int http_parse_status_line(struct http_parser_t *http, const char* data, size_t len)
{
	enum { 
		SM_STATUS_START = SM_START_LINE, 
		SM_STATUS_VERSION,
		SM_STATUS_SP1,
		SM_STATUS_CODE, 
		SM_STATUS_SP2,
		SM_STATUS_REASON,
		SM_STATUS_SP3,
		SM_STATUS_CR,
		SM_STATUS_LF,
	};

	char c;
	size_t i;
	size_t pos;
	size_t *v[6];
	v[0] = &http->header.name.pos; // version
	v[1] = &http->header.name.len;
	v[2] = &http->header.value.pos; // status code
	v[3] = &http->header.value.len;
	v[4] = &http->u.reply.reason.pos;
	v[5] = &http->u.reply.reason.len;

	pos = http->raw_size;
	for(i = 0; i < len; i++)
	{
		c = data[i];
		switch(http->stateM)
		{
		case SM_STATUS_START:
		case SM_STATUS_SP1:
		case SM_STATUS_SP2:
			if (' ' != c && '\t' != c)
			{
				*(v[http->stateM - SM_STATUS_START]) = pos + i;
				http->stateM += 1; // next state
				i -= 1; // go back
			}
			break;

		case SM_STATUS_VERSION:
		case SM_STATUS_CODE:
		case SM_STATUS_REASON:
			if (' ' == c || '\t' == c || '\r' == c || '\n' == c)
			{
				*(v[http->stateM - SM_STATUS_START]) = pos + i - *(v[http->stateM - SM_STATUS_START - 1]);
				http->stateM += 1; // next state
				i -= 1; // go back
			}
			break;

		case SM_STATUS_SP3:
			switch (c)
			{
			case ' ':
			case '\t':
				break; // continue

			case '\r':
				http->stateM = SM_STATUS_CR;
				break;

			case '\n':
				http->stateM = SM_STATUS_LF;
				i -= 1; // go back
				break;

			default:
				http->stateM = SM_STATUS_REASON;
				break;
			}
			break;

		case SM_STATUS_CR:
			if ('\n' != c)
			{
				assert(0);
				return -1;
			}
			http->stateM = SM_STATUS_LF;
			i -= 1; // go back
			break;

		case SM_STATUS_LF:
			assert('\n' == c);

			// copy response line
			if (0 != http_rawdata(http, data, i))
				return -ENOMEM;

			// H3.1 HTTP Version (p13)
			// HTTP-Version = "HTTP" "/" 1*DIGIT "." 1*DIGIT
			if (http->header.name.len < 5 || 3 != sscanf(http->raw + http->header.name.pos, "%16[^/]/%d.%d", http->protocol, &http->vermajor, &http->verminor)
				// H6.1.1 Status Code and Reason Phrase (p26)
				// The Status-Code element is a 3-digit integer result code
				|| http->header.value.len != 3 /*|| http->u.reply.reason.len < 1*/ )
			{
				assert(0);
				return -1;
			}

			http->raw[http->u.reply.reason.pos + http->u.reply.reason.len] = '\0';
			http->raw[http->header.value.pos + http->header.value.len] = '\0';
			http->u.reply.code = atoi(http->raw + http->header.value.pos);
			//assert(1 == http->vermajor || 2 == http->vermajor);
			//assert(1 == http->verminor || 0 == http->verminor);
			http->stateM = SM_HEADER;
			i += 1; // skip '\n'
			return (int)i;

		default:
			assert(0);
			return -1; // invalid
		}
	}

	// copy response line
	if (0 != http_rawdata(http, data, i))
		return -ENOMEM;
	return (int)i;
}

// H4.2 Message Headers
// message-header = field-name ":" [ field-value ]
// field-name = token
// field-value = *( field-content | LWS )
// field-content = <the OCTETs making up the field-value
//					and consisting of either *TEXT or combinations
//					of token, separators, and quoted-string>
//
// empty value
// e.g. x-wap-profile: \r\nx-forwarded-for: 10.25.110.244, 115.168.35.85\r\n
static int http_parse_header_line(struct http_parser_t *http, const char* data, size_t len)
{
	enum { 
		SM_HEADER_START = SM_HEADER, 
		SM_HEADER_NAME,
		SM_HEADER_SP1,
		SM_HEADER_SEPARATOR,
		SM_HEADER_SP2,
		SM_HEADER_VALUE,
		SM_HEADER_SP3,
		SM_HEADER_CR,
		SM_HEADER_LF,
	};

	char c;
	size_t i;
	size_t j;
	size_t pos;
	size_t dummy;
	size_t *v[6];

	v[0] = &http->header.name.pos;
	v[1] = &http->header.name.len;
	v[2] = &dummy;
	v[3] = &dummy;
	v[4] = &http->header.value.pos;
	v[5] = &http->header.value.len;

	pos = http->raw_size;
	for(i = 0; i < len; i++)
	{
		c = data[i];
		switch (http->stateM)
		{
		case SM_HEADER_START:
			switch (c)
			{
			case ' ':
			case '\t':
				// H2.2 Basic Rules (p12)
				// HTTP/1.1 header field values can be folded onto multiple lines if the continuation line begins with a space or
				// horizontal tab.All linear white space, including folding, has the same semantics as SP.A recipient MAY replace any
				// linear white space with a single SP before interpreting the field value or forwarding the message downstream.
				http->header.name.pos = 0; // use for multiple lines flag
				http->stateM = SM_HEADER_SP2; // next state
				break;

			case '\r':
				http->header.value.pos = 0; // use for header end flag
				http->stateM = SM_HEADER_CR;
				break;

			case '\n':
				http->header.value.pos = 0; // use for header end flag
				http->stateM = SM_HEADER_LF;
				i -= 1; // go back
				break;

			default:
				http->header.name.pos = pos + i;
				http->stateM += 1; // next state
				i -= 1; // go back
				break;
			}
			break;

		case SM_HEADER_SP1:
		case SM_HEADER_SP2:
			if (' ' != c && '\t' != c)
			{
				*(v[http->stateM - SM_HEADER_START]) = pos + i;
				http->stateM += 1; // next state
				i -= 1; // go back
			}
			break;

		case SM_HEADER_NAME:
		case SM_HEADER_VALUE:
			if (' ' == c || '\t' == c || '\r' == c || '\n' == c || (':' == c && SM_HEADER_NAME == http->stateM))
			{
				*(v[http->stateM - SM_HEADER_START]) = pos + i - *(v[http->stateM - SM_HEADER_START - 1]);
				http->stateM += 1; // next state
				i -= 1; // go back
			}
			break;

		case SM_HEADER_SEPARATOR:
			if (':' != c)
			{
				assert(0);
				return -1;
			}

			// accept
			http->stateM += 1; // next state
			break;

		case SM_HEADER_SP3:
			switch (c)
			{
			case ' ':
			case '\t':
				break; // continue

			case '\r':
				http->stateM = SM_HEADER_CR;
				break;

			case '\n':
				http->stateM = SM_HEADER_LF;
				i -= 1; // go back
				break;

			default:
				http->stateM = SM_HEADER_VALUE;
				break;
			}
			break;

		case SM_HEADER_CR:
			if ('\n' != c)
			{
				assert(0);
				return -1;
			}
			http->stateM = SM_HEADER_LF;
			i -= 1; // go back
			break;

		case SM_HEADER_LF:
			assert('\n' == c);

			// copy header line
			if (0 != http_rawdata(http, data, i))
				return -ENOMEM;

			if (0 == http->header.value.pos)
			{
				http->raw_header_offset = http->raw_size;
				http->stateM = SM_BODY;
				i += 1; // skip '\n'
				return (int)i;
			}

			if (0 == http->header.name.pos)
			{
				// multiple lines header
				struct http_header_t* h;

				if (http->header_size < 1)
				{
					assert(0);
					return -1;
				}

				h = &http->headers[http->header_size - 1];
				for (j = h->value.len; j < http->header.value.pos; j++)
				{
					http->raw[j] = ' '; // replace with SP
				}
				h->value.len = http->header.value.pos - h->value.pos + http->header.value.len;
				http->raw[h->value.pos + h->value.len] = '\0';

				http_header_handler(http, h->name.pos, h->value.pos); // handle
			}
			else
			{
				if (http->header.name.len < 1)
				{
					assert(0);
					return -1;
				}

				if (0 != http_header_add(http, &http->header))
					return -1;

				http_header_handler(http, http->header.name.pos, http->header.value.pos); // handle
			}

			http->stateM = SM_HEADER; // continue
			i += 1; // skip '\n'
			return (int)i;

		default:
			assert(0);
			return -1;
		}
	}

	// copy header line
	if (0 != http_rawdata(http, data, i))
		return -ENOMEM;
	return (int)i;
}

// H3.6.1 Chunked Transfer Coding
// Chunked-Body		= *chunk
//					  last-chunk
//					  trailer
//					  CRLF
//	chunk			= chunk-size [ chunk-extension ] CRLF
//					  chunk-data CRLF
//	chunk-size		= 1*HEX
//	last-chunk		= 1*("0") [ chunk-extension ] CRLF
//	chunk-extension	= *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
//	chunk-ext-name	= token
//	chunk-ext-val	= token | quoted-string
//	chunk-data		= chunk-size(OCTET)
//	trailer			= *(entity-header CRLF)
static int http_parse_chunked(struct http_parser_t *http, const char* data, size_t len)
{
	enum {
		CHUNK_START = SM_BODY,
		CHUNK_SIZE,
		CHUNK_EXTENSION,
		CHUNK_EXTENSION_CR,
		CHUNK_DATA,
		CHUNK_DATA_CR,
		CHUNK_TRAILER_START,
		CHUNK_TRAILER,
		CHUNK_TRAILER_CR,
		CHUNK_END_CR,
	};

	char c;
	size_t i;
	size_t n;
	assert(is_transfer_encoding_chunked(http));

	for(i = 0; i < len && SM_DONE != http->stateM; i++)
	{
		c = data[i];

		switch(http->stateM)
		{
		case CHUNK_START:
			assert(0 == http->chunk.len);
			if('0' <= c && c <= '9')
			{
				http->chunk.len = c - '0';
			}
			else if('a' <= c && c <= 'f')
			{
				http->chunk.len = c - 'a' + 10;
			}
			else if('A' <= c && c <= 'F')
			{
				http->chunk.len = c - 'A' + 10;
			}
			else
			{
				assert(0);
				return -1;
			}

			http->stateM = CHUNK_SIZE;
			break;

		case CHUNK_SIZE:
			if('0' <= c && c <= '9')
			{
				http->chunk.len = http->chunk.len * 16 + (c - '0');
			}
			else if('a' <= c && c <= 'f')
			{
				http->chunk.len = http->chunk.len * 16 + (c - 'a' + 10);
			}
			else if('A' <= c && c <= 'F')
			{
				http->chunk.len = http->chunk.len * 16 + (c - 'A' + 10);
			}
			else
			{
				switch(c)
				{
				case '\t':
				case ' ':
				case ';':
					http->stateM = CHUNK_EXTENSION;
					break;

				case '\r':
					http->stateM = CHUNK_EXTENSION_CR;
					break;

				case '\n':
					http->stateM = CHUNK_DATA;
					break;

				default:
					assert(0);
					return -1;
				}
			}
			break;

		case CHUNK_EXTENSION:
			switch(c)
			{
			case '\r':
				http->stateM = CHUNK_EXTENSION_CR;
				break;

			case '\n':
				http->stateM = CHUNK_DATA;
				break;
			}
			break;

		case CHUNK_EXTENSION_CR:
			if('\n' != c)
			{
				assert(0);
				return -1;
			}

			http->stateM = http->chunk.len ? CHUNK_DATA : CHUNK_TRAILER_START;
			break;

		case CHUNK_DATA:
			assert(http->chunk.len > 0);
			assert(http->chunk.loaded <= http->chunk.len);
			n = http->chunk.len - http->chunk.loaded;
			n = i + n > len ? len - i : n;

			// copy chunk data
			if (n > 0 && 0 != http_body(http, data + i, n))
				return -ENOMEM;

			i += n - 1;
			http->chunk.loaded += n;
			if (http->chunk.loaded == http->chunk.len)
				http->stateM = CHUNK_DATA_CR;
			break;

		case CHUNK_DATA_CR:
			switch(c)
			{
			case '\r':
				break;

			case '\n':
				http->stateM = CHUNK_START;
				http->chunk.len = 0;
				http->chunk.loaded = 0;
				break;

			default:
				assert(0);
			}
			break;

		case CHUNK_TRAILER_START:
			switch (c)
			{
			case '\r':
				http->stateM = CHUNK_END_CR;
				break;

			case '\n':
				http->stateM = SM_DONE;
				break;

			default:
				http->stateM = CHUNK_TRAILER;
				break;
			}
			break;

		case CHUNK_TRAILER:
			switch(c)
			{
			case '\r':
				http->stateM = CHUNK_TRAILER_CR;
				break;

			case '\n':
				http->stateM = CHUNK_TRAILER_START;
				break;

			default:
				break;
			}
			break;

		case CHUNK_TRAILER_CR:
			if('\n' != c)
			{
				assert(0);
				return -1;
			}
			http->stateM = CHUNK_TRAILER_START;
			break;
			
		case CHUNK_END_CR:
			if('\n' != c)
			{
				assert(0);
				return -1;
			}
			http->stateM = SM_DONE;
			break;
		}
	}

	assert(i == len);
	return (int)i;
}

struct http_parser_t* http_parser_create(enum HTTP_PARSER_MODE mode, void(*ondata)(void* param, const void* data, int len), void* param)
{
	struct http_parser_t *http;
	http = (struct http_parser_t*)malloc(sizeof(struct http_parser_t));
	if(!http)
		return NULL;

	memset(http, 0, sizeof(struct http_parser_t));
	http->callback = ondata;
	http->param = param;
	http->request = mode;
	http_parser_clear(http);
	return http;
}

int http_parser_destroy(struct http_parser_t* http)
{
	if(http->raw)
	{
		assert(http->raw_capacity > 0);
		free(http->raw);
		http->raw = 0;
		http->raw_size = 0;
		http->raw_capacity = 0;
	}

	if(http->headers)
	{
		assert(http->header_capacity > 0);
		free(http->headers);
		http->headers = 0;
		http->header_size = 0;
		http->header_capacity = 0;
	}

	free(http);
	return 0;
}

void http_parser_clear(struct http_parser_t* http)
{
	memset(&http->u.req, 0, sizeof(http->u.req));
	memset(&http->u.reply, 0, sizeof(http->u.reply));
	memset(&http->chunk, 0, sizeof(struct http_chunk_t));
	http->stateM = SM_START_LINE;
	http->raw_header_offset = 0;
	http->raw_body_length = 0;
	http->raw_size = 0;
	http->header_size = 0;
	http->content_length = -1;
	http->connection_close = -1;
	http->content_encoding = 0;
	http->transfer_encoding = 0;
	http->cookie = 0;
	http->location = 0;
}

int http_parser_input(struct http_parser_t* http, const void* data, size_t *bytes)
{
	enum { INPUT_NEEDMORE = 1, INPUT_DONE = 0, INPUT_HEADER = 2, };

	int r;
	size_t n;
	const char* ptr, *end;
	ptr = (const char*)data;
	end = ptr + *bytes;

	//// save raw data
	//r = http_rawdata(http, data, *bytes);
	//if(0 != r)
	//{
	//	assert(r < 0);
	//	return r;
	//}

	r = 0;
	if(SM_START_LINE <= http->stateM && http->stateM < SM_HEADER)
	{
		r = HTTP_PARSER_REQUEST == http->request ? http_parse_request_line(http, ptr, end - ptr) : http_parse_status_line(http, ptr, end - ptr);
		if (r < 0)
			return r;

		ptr += r;
	}

	while(end > ptr && SM_HEADER <= http->stateM && http->stateM < SM_BODY)
	{
		r = http_parse_header_line(http, ptr, end - ptr);
		if (r < 0)
			return r;
		ptr += r;
	}

	if(SM_BODY <= http->stateM && http->stateM < SM_DONE)
	{
		if(is_transfer_encoding_chunked(http))
		{
			r = http_parse_chunked(http, ptr, end - ptr);
			ptr += r;
		}
		else
		{
			if(-1 == http->content_length)
			{
				// 4.3 Message Body: All 1xx(informational), 204 (no content), and 304 (not modified) responses MUST NOT include a message-body
				// 4.4 Message Length: HEAD/1xx/204/304

				if(HTTP_PARSER_RESPONSE == http->request && 0 == strcasecmp("HTTP", http->protocol)
					&& http->u.reply.code != 204 && http->u.reply.code != 304
					&& (http->u.reply.code < 100 || http->u.reply.code >= 200))
				{
					r = http_body(http, ptr, end - ptr);
					ptr = end;

					// H4.4 Message Length, section 5, server closing the connection
					// receive all until socket closed
					if(0 == *bytes /*|| http->raw_size == http->offset*/)
					{
						//http->content_length = http->raw_size; // - http->header ??
						http->stateM = SM_DONE;
					}
				}
				else
				{
					// RFC2326 RTSP 4.4 Message Length
					// 2. If a Content-Length header field is not present, a value of zero is assumed.
					// 3. By the server closing the connection. (Closing the connection cannot be used to 
					//    indicate the end of a request body, since that would leave no possibility for the 
					//    server to send back a response.)
					http->content_length = 0;
					http->stateM = SM_DONE;
				}
			}
			else
			{
				assert(http->raw_body_length <= http->content_length);
				n = end - ptr + http->raw_body_length > http->content_length ? (size_t)(http->content_length - http->raw_body_length) : end - ptr;
				r = http_body(http, ptr, n);
				ptr += n;
				if(http->raw_body_length >= http->content_length)
					http->stateM = SM_DONE;
			}
		}
	}

	if(r < 0)
		return r;

	*bytes = 0;
	if (SM_DONE == http->stateM)
	{
		assert(http->content_length < 0 || http->raw_body_length == http->content_length);
		*bytes = end - ptr;
	}
	return http->stateM == SM_DONE ? INPUT_DONE : (SM_BODY <= http->stateM ? INPUT_HEADER : INPUT_NEEDMORE);
}

size_t http_get_max_size(void)
{
	return s_body_max_size;
}

int http_set_max_size(size_t bytes)
{
	s_body_max_size = bytes;
	return 0;
}

int http_get_version(const struct http_parser_t* http, char protocol[64], int *major, int *minor)
{
	assert(http->stateM>=SM_BODY);
	assert(64 >= sizeof(http->protocol));
	strcpy(protocol, http->protocol);
	*major = http->vermajor;
	*minor = http->verminor;
	return 0;
}

int http_get_status_code(const struct http_parser_t* http)
{
	assert(http->stateM>=SM_BODY);
	assert(HTTP_PARSER_RESPONSE == http->request);
	return http->u.reply.code;
}

const char* http_get_status_reason(const struct http_parser_t* http)
{
	assert(http->stateM>=SM_BODY);
	assert(HTTP_PARSER_RESPONSE == http->request);
	return http->raw + http->u.reply.reason.pos;
}

const char* http_get_request_method(const struct http_parser_t* http)
{
	assert(http->stateM>=SM_BODY);
	assert(HTTP_PARSER_REQUEST == http->request);
	return http->raw + http->u.req.method.pos;
}

const char* http_get_request_uri(const struct http_parser_t* http)
{
	assert(http->stateM>=SM_BODY);
	assert(HTTP_PARSER_REQUEST == http->request);
	return http->raw + http->u.req.uri.pos;
}

const void* http_get_content(const struct http_parser_t* http)
{
	assert(!http->callback);
	assert(http->stateM>=SM_BODY);
	assert(http->raw_header_offset + http->raw_body_length == http->raw_size);
	return http->raw + http->raw_header_offset;
}

int http_get_header_count(const struct http_parser_t* http)
{
	assert(http->stateM>=SM_BODY);
	return http->header_size;
}

int http_get_header(const struct http_parser_t* http, int idx, const char** name, const char** value)
{
	assert(http->stateM>=SM_BODY);

	if(idx < 0 || idx >= http->header_size)
		return EINVAL;

	*name = http->raw + http->headers[idx].name.pos;
	*value = http->raw + http->headers[idx].value.pos;
	return 0;
}

const char* http_get_header_by_name(const struct http_parser_t* http, const char* name)
{
	int i;
	assert(http->stateM>=SM_BODY);

	for(i = 0; i < http->header_size; i++)
	{
		// TODO: 
		// RFC-2616 4.2 Message Headers p22
		// Multiple message-header fields with the same field-name MAY be present in a message
		if(0 == strcasecmp(http->raw + http->headers[i].name.pos, name))
			return http->raw + http->headers[i].value.pos;
	}

	return NULL; // not found
}

int http_get_header_by_name2(const struct http_parser_t* http, const char* name, int *value)
{
	int i;
	assert(http->stateM>=SM_BODY);

	for(i = 0; i < http->header_size; i++)
	{
		if(0 == strcasecmp(http->raw + http->headers[i].name.pos, name))
		{
			*value = atoi(http->raw + http->headers[i].value.pos);
			return 0;
		}
	}

	return -1;
}

int64_t http_get_content_length(const struct http_parser_t* http)
{
	assert(http->stateM>=SM_BODY);
	if(-1 == http->content_length && http->callback)
		return http->raw_body_length;
	return http->content_length;
}

int http_get_connection(const struct http_parser_t* http)
{
	assert(http->stateM>=SM_BODY);
	return http->connection_close;
}

const char* http_get_content_type(const struct http_parser_t* http)
{
	return http_get_header_by_name(http, "Content-Type");
}

const char* http_get_content_encoding(const struct http_parser_t* http)
{
	assert(http->stateM>=SM_BODY);
	if(0 == http->content_encoding)
		return NULL;
	return http->raw + http->content_encoding;
}

const char* http_get_transfer_encoding(const struct http_parser_t* http)
{
	assert(http->stateM>=SM_BODY);
	if(0 == http->transfer_encoding)
		return NULL;
	return http->raw + http->transfer_encoding;
}

const char* http_get_cookie(const struct http_parser_t* http)
{
	assert(http->stateM>=SM_BODY);
	if(0 == http->cookie)
		return NULL;
	return http->raw + http->cookie;
}

const char* http_get_location(const struct http_parser_t* http)
{
	assert(http->stateM>=SM_BODY);
	assert(HTTP_PARSER_RESPONSE == http->request);
	if(0 == http->location)
		return NULL;
	return http->raw + http->location;
}
