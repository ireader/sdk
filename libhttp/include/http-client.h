#ifndef _http_client_h_
#define _http_client_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct http_transport_t;
typedef struct http_client_t http_client_t;

enum { HTTP_READ_FLAGS_WHOLE = 0x01, };

struct http_header_t
{
	const char* name;
	const char* value;
};

/// HTTP response callback(on recevied response http header), use http_client_read to read response content
/// @param[in] code 0-ok, other-error
/// @param[in] http_status_code http response status code
/// @param[in] http_content_length -1-chunked, >=0-content length, other-undefined
typedef void (*http_client_onresponse)(void *param, int code, int http_status_code, int64_t http_content_length);

/// create HTTP client
/// @param[in] transport NULL use default transport
/// @param[in] scheme http/https
/// @param[in] ip HTTP service ip
/// @param[in] port HTTP Service port
/// @return http handler
http_client_t *http_client_create(struct http_transport_t* transport, const char* scheme, const char* ip, unsigned short port);
void http_client_destroy(http_client_t* http);

/// @return http transport connection(session)
void* http_client_getconnection(http_client_t* http);

/// HTTP socket timeout
/// @param[in] http HTTP handler created by http_client_create
/// @param[in] conn socket connect timeout(MS)
/// @param[in] recv socket read timeout(MS)
/// @param[in] send socket write timeout(MS)
void http_client_set_timeout(http_client_t* http, int conn, int recv, int send);

/// HTTP 3xx redirect rule check
/// urls follow urls, from orgin[0] -> last redirect url
/// @param[in] onredirect return 1-enable redirect, 0-disable redirect
void http_client_set_redirect(http_client_t* http, int (*onredirect)(void* param, const char* urls[], int n), void* param);

/// HTTP GET Request
/// r = http_client_get(handle, "/webservice/api/version", NULL, 0, OnVersion, param)
/// @param[in] http HTTP handler created by http_client_create
/// @param[in] uri Request URI(include parameter and fragment)
/// @param[in] headers HTTP request header(such as Cookie, Host, Content-Type)
/// @param[in] n HTTP request header count
/// @param[in] onreply user-defined callback function(maybe callback in other thread if in aio mode)
/// @param[in] param user-defined callback parameter
int http_client_get(http_client_t* http, const char* uri, const struct http_header_t *headers, size_t n, http_client_onresponse onreply, void* param);

/// HTTP POST Request
/// m = strdup("what's your name?");
/// r = http_client_post(handle, "/webservice/api/hello", NULL, 0, m, strlen(m), OnHello, param)
/// @param[in] http HTTP handler created by http_client_create
/// @param[in] uri Request URI(include parameter and fragment)
/// @param[in] headers HTTP request header(such as Cookie, Host, Content-Type)
/// @param[in] n HTTP request header count
/// @param[in] msg POST content(memory must valid before callback)
/// @param[in] bytes POST content size in byte
/// @param[in] onreply user-defined callback function(maybe callback in other thread if in aio mode)
/// @param[in] param user-defined callback parameter
int http_client_post(http_client_t* http, const char* uri, const struct http_header_t *headers, size_t n, const void* msg, size_t bytes, http_client_onresponse onreply, void* param);

// Response

/// Get server response HTTP header field by name
/// @param[in] http HTTP client handler
/// @param[in] name request http header field name
/// @return NULL-don't found field, other-header value
const char* http_client_get_header(http_client_t* http, const char *name);

/// read server response data
/// @param[in] http HTTP client handle
/// @param[out] data response body
/// @param[out] bytes response body size, 0-all done
/// @param[in] flags 0x01-fill bytes, other-undefined
/// @return 0-ok, other-error
int http_client_read(http_client_t* http, void *data, size_t bytes, int flags, void (*onread)(void* param, int code, void* data, size_t bytes), void* param);

#ifdef __cplusplus
}
#endif
#endif /* !_http_client_h_ */
