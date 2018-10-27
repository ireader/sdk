#ifndef _http_client_h_
#define _http_client_h_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_client_t http_client_t;

struct http_header_t
{
	const char* name;
	const char* value;
};

typedef void(*http_client_onreply)(void *param, int code);

/// create HTTP client
/// @param[in] ip HTTP service ip
/// @param[in] port HTTP Service port
/// @param[in] flags 0-aio, 1-block io
/// @return http handler
http_client_t *http_client_create(const char* ip, unsigned short port, int flags);
void http_client_destroy(http_client_t* http);

/// HTTP socket timeout
/// @param[in] http HTTP handler created by http_client_create
/// @param[in] conn socket connect timeout(MS)
/// @param[in] recv socket read timeout(MS)
/// @param[in] send socket write timeout(MS)
void http_client_set_timeout(http_client_t* http, int conn, int recv, int send);

/// HTTP GET Request
/// r = http_client_get(handle, "/webservice/api/version", NULL, 0, OnVersion, param)
/// @param[in] http HTTP handler created by http_client_create
/// @param[in] uri Request URI(include parameter and fragment)
/// @param[in] headers HTTP request header(such as Cookie, Host, Content-Type)
/// @param[in] n HTTP request header count
/// @param[in] onreply user-defined callback function(maybe callback in other thread if in aio mode)
/// @param[in] param user-defined callback parameter
int http_client_get(http_client_t* http, const char* uri, const struct http_header_t *headers, size_t n, http_client_onreply onreply, void* param);

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
int http_client_post(http_client_t* http, const char* uri, const struct http_header_t *headers, size_t n, const void* msg, size_t bytes, http_client_onreply onreply, void* param);

// Response

/// Get server response HTTP header field by name
/// @param[in] http HTTP client handler
/// @param[in] name request http header field name
/// @return NULL-don't found field, other-header value
const char* http_client_get_header(http_client_t* http, const char *name);

/// Get server response data(raw data)
/// @param[in] http HTTP client handle
/// @param[out] content response body pointer(don't need free)
/// @param[out] bytes response body size
/// @return 0-ok, other-error
int http_client_get_content(http_client_t* http, const void **content, size_t *bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_http_client_h_ */
