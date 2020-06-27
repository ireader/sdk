#ifndef _http_parser_h_
#define _http_parser_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_parser_t http_parser_t;

enum HTTP_PARSER_MODE { 
	HTTP_PARSER_RESPONSE = 0,   // HTTP/1.1 200 OK
	HTTP_PARSER_REQUEST = 1		// POST /uri HTTP/1.1
};

/// get/set maximum body size(global setting)
/// @param[in] bytes 0-unlimited, other-limit bytes
int http_set_max_size(size_t bytes);
size_t http_get_max_size(void);

/// create
/// @param[in] mode 1-server mode, 0-client mode
/// @return parser instance
http_parser_t* http_parser_create(enum HTTP_PARSER_MODE mode, void(*ondata)(void* param, const void* data, int len), void* param);

/// destroy
/// @return 0-ok, other-error
int http_parser_destroy(http_parser_t* parser);

/// clear state
void http_parser_clear(http_parser_t* parser);

/// input data
/// @param[in] data content
/// @param[in,out] bytes out-remain bytes
/// @return 1-need more data, 0-receive done, <0-error
int http_parser_input(http_parser_t* parser, const void* data, size_t *bytes);

/// HTTP start-line
int http_get_version(const http_parser_t* parser, char protocol[64], int *major, int *minor);
int http_get_status_code(const http_parser_t* parser);
const char* http_get_status_reason(const http_parser_t* parser);
const char* http_get_request_uri(const http_parser_t* parser);
const char* http_get_request_method(const http_parser_t* parser);

/// HTTP body(use with http_get_content_length)
/// Get HTTP body if http_parser_create without callback, otherwise, it's invalid
const void* http_get_content(const http_parser_t* parser);

/// HTTP headers
/// @return 0-ok, other-error
int http_get_header_count(const http_parser_t* parser);
/// @return 0-ok, <0-don't have header
int http_get_header(const http_parser_t* parser, int idx, const char** name, const char** value);
/// @return NULL-don't found header, other-header value
const char* http_get_header_by_name(const http_parser_t* parser, const char* name);
/// @return 0-ok, <0-don't have header
int http_get_header_by_name2(const http_parser_t* parser, const char* name, int *value);
/// @return >=0-content-length, <0-don't have content-length header
int64_t http_get_content_length(const http_parser_t* parser);
/// @return 1-close, 0-keep-alive, <0-don't have connection header
int http_get_connection(const http_parser_t* parser);
/// @return Content-Type, NULL-don't have this header
const char* http_get_content_type(const http_parser_t* parser);
/// @return Content-Encoding, NULL-don't have this header
const char* http_get_content_encoding(const http_parser_t* parser);
/// @return Transfer-Encoding, NULL-don't have this header
const char* http_get_transfer_encoding(const http_parser_t* parser);
/// @return Set-Cookie, 0-don't have this header
const char* http_get_cookie(const http_parser_t* parser);
/// @return Location, 0-don't have this header
const char* http_get_location(const http_parser_t* parser);

#ifdef __cplusplus
}
#endif
#endif /* !_http_parser_h_ */
