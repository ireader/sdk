#ifndef _http_client_h_
#define _http_client_h_

#ifdef __cplusplus
extern "C" {
#endif

struct http_header_t
{
	const char* name;
	const char* value;
};

int http_client_init(void);
int http_client_cleanup(void);

/// create HTTP client
/// @param[in] ip HTTP service ip
/// @param[in] port HTTP Service port
/// @param[in] flags 0-aio, 1-block io
/// @return http handler
void *http_client_create(const char* ip, unsigned short port, int flags);
void http_client_destroy(void* client);

/// HTTP connection recycle
/// @param[in] client HTTP handler created by http_client_create
void http_client_recycle(void* client);

typedef void (*http_client_response)(void *param, void *http, int code);

/// HTTP GET Request
/// r = http_client_get(handle, "/webservice/api/version", NULL, 0, OnVersion, param)
/// @param[in] client HTTP handler created by http_client_create
/// @param[in] uri Request URI(include parameter and fragment)
/// @param[in] headers HTTP request header(such as Cookie, Host, Content-Type)
/// @param[in] n HTTP request header count
/// @param[in] callback user-defined callback function(maybe callback in other thread if in aio mode)
/// @param[in] param user-defined callback parameter
int http_client_get(void* client, const char* uri, const struct http_header_t *headers, size_t n, http_client_response callback, void *param);

/// HTTP POST Request
/// m = strdup("what's your name?");
/// r = http_client_post(handle, "/webservice/api/hello", NULL, 0, m, strlen(m), OnHello, param)
/// @param[in] client HTTP handler created by http_client_create
/// @param[in] uri Request URI(include parameter and fragment)
/// @param[in] headers HTTP request header(such as Cookie, Host, Content-Type)
/// @param[in] n HTTP request header count
/// @param[in] msg POST content(memory must valid before callback)
/// @param[in] bytes POST content size in byte
/// @param[in] callback user-defined callback function(maybe callback in other thread if in aio mode)
/// @param[in] param user-defined callback parameter
int http_client_post(void* client, const char* uri, const struct http_header_t *headers, size_t n, const void* msg, size_t bytes, http_client_response callback, void *param);

// Response

/// Get server response HTTP header field by name
/// @param[in] http HTTP client handler
/// @param[in] name request http header field name
/// @return NULL-don't found field, other-header value
const char* http_client_get_header(void* http, const char *name);

/// Get server response data(raw data)
/// @param[in] http HTTP client handle
/// @param[out] content response body pointer(don't need free)
/// @param[out] bytes response body size
/// @return 0-ok, other-error
int http_client_get_content(void* http, const void **content, size_t *bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_http_client_h_ */
