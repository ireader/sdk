#include "http-parser.h"
#include <string.h>
#include <assert.h>

static void http_parser_request_test(void)
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

static void http_parser_response_test(void)
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
	assert(0 == http_get_version(parser, protocol, &major, &minor) && 1 == major && 0 == minor && 0 == strcmp("RTSP", protocol));
	assert(0 == strcmp(http_get_header_by_name(parser, "CSeq"), "2"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Date"), "Mon, Jun 26 2017 18:38:09 GMT"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Transport"), "RTP/AVP;unicast;destination=192.168.31.1;source=192.168.31.132;client_port=33900-33901;server_port=6970-6971"));
	assert(0 == strcmp(http_get_header_by_name(parser, "Session"), "93C429BE;timeout=65"));
	http_parser_destroy(parser);
}

void http_parser_test(void)
{
	http_parser_request_test();
	http_parser_response_test();
}
