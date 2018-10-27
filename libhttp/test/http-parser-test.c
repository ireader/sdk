#include "http-parser.h"
#include <string.h>
#include <assert.h>

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
	parser = http_parser_create(HTTP_PARSER_SERVER);
	assert(0 == http_parser_input(parser, s, &n));
	assert(0 == http_get_version(parser, protocol, &major, &minor) && 1 == major && 1 == minor && 0 == strcmp("HTTP", protocol));
	assert(0 == strcmp(http_get_request_uri(parser), "/"));
	assert(0 == strcmp(http_get_request_method(parser), "GET"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Host"), "www.baidu.com"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Date"), "Mon, Jun 26 2017 18:38:09 GMT"));
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
	parser = http_parser_create(HTTP_PARSER_CLIENT);
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
	parser = http_parser_create(HTTP_PARSER_SERVER);
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
	parser = http_parser_create(HTTP_PARSER_CLIENT);
	assert(0 == http_parser_input(parser, s, &n));
	assert(0 == http_get_version(parser, protocol, &major, &minor) && 2 == major && 0 == minor && 0 == strcmp("SIP", protocol));
	assert(200 == http_get_status_code(parser));
	assert(0 == http_get_header(parser, 0, &name, &value) && 0 == strcmp("Via", name) && 0 == strcmp("SIP/2.0/UDP server10.biloxi.com;branch=z9hG4bKnashds8;received=192.0.2.3", value));
	assert(0 == http_get_header(parser, 1, &name, &value) && 0 == strcmp("Via", name) && 0 == strcmp("SIP/2.0/UDP bigbox3.site3.atlanta.com;branch=z9hG4bK77ef4c2312983.1;received=192.0.2.2", value));
	assert(0 == http_get_header(parser, 2, &name, &value) && 0 == strcmp("Via", name) && 0 == strcmp("SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds ;received=192.0.2.1", value));
	assert(0 == strcmp(http_get_content_type(parser), "application/sdp"));
	http_parser_destroy(parser);
}

void http_parser_test(void)
{
	http_request_test();
	rtsp_response_test();
	sip_response_test();
}
