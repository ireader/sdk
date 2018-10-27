#ifndef _http_server_h_
#define _http_server_h_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_server_t http_server_t;
typedef struct http_session_t http_session_t;

struct http_vec_t
{
	const void* data;
	size_t bytes;
};

/// @param[in] code HTTP status-code(200-OK, 301-Move Permanently, ...)
/// @return 0-ok(continue read), other-close socket
typedef int (*http_server_onsend)(void* param, int code, size_t bytes);

http_server_t* http_server_create(const char* ip, int port);
int http_server_destroy(http_server_t* http);

// Request

/// Get client request HTTP header field by name
/// @param[in] session handle callback session parameter
/// @param[in] name request http header field name
/// @return NULL-don't found field, other-header value
const char* http_server_get_header(http_session_t* session, const char *name);

/// Get client post data(raw data)
/// @param[in] session handle callback session parameter
/// @param[out] content data pointer(don't need free)
/// @param[out] length data size
/// @return 0-ok, other-error
int http_server_get_content(http_session_t* session, void **content, size_t *length);

/// Get client ip/port
/// @param[in] session handle callback session parameter
/// @param[out] ip client bind ip
/// @param[out] port client bind port
/// @return 0-ok, other-error
int http_server_get_client(http_session_t* session, char ip[65], unsigned short *port);

// Response

/// Reply
/// @param[in] session handle callback session parameter
/// @param[in] code HTTP status-code(200-OK, 301-Move Permanently, ...)
/// @return 0-ok, other-error
int http_server_send(http_session_t* session, int code, const void* data, size_t bytes, http_server_onsend onsend, void* param);

/// Reply
/// @param[in] session handle callback session parameter
/// @param[in] code HTTP status-code(200-OK, 301-Move Permanently, ...)
/// @param[in] vec bundle array
/// @param[in] num array elementary number
/// @return 0-ok, other-error
int http_server_send_vec(http_session_t* session, int code, const struct http_vec_t* vec, int num, http_server_onsend onsend, void* param);

/// Reply a server side file(must be local regular file)
/// @param[in] session handle callback session parameter
/// @param[in] localpath local regular file pathname
/// @return 0-ok, other-error
int http_server_sendfile(http_session_t* session, const char* localpath, http_server_onsend onsend, void* param);

/// Set response http header field(every reply must set it again)
/// @param[in] session handle callback session parameter
/// @param[in] name header name
/// @param[in] value header value
/// @return 0-ok, other-error
int http_server_set_header(http_session_t* session, const char* name, const char* value);

/// Set response http header field(every reply must set it again)
/// @param[in] session handle callback session parameter
/// @param[in] name header name
/// @param[in] value header value
/// @return 0-ok, other-error
int http_server_set_header_int(http_session_t* session, const char* name, int value);

/// Set response http header Content-Type field value(every reply must set it again)
/// @param[in] session handle callback session parameter
/// @param[in] value content-type value
/// @return 0-ok, other-error
int http_server_set_content_type(http_session_t* session, const char* value);


// Handler

/// Set request handler(call on every request)
/// @param[in] param user-defined parameter
/// @param[in] session http session id(use for get request info and set reply)
/// @param[in] method http request method(get/post...)
/// @param[in] path http request uri(e.g. /api/a.html?xxxx)
/// @return 0-ok, other-error
typedef int (*http_server_handler)(void* param, http_session_t* session, const char* method, const char* path);

/// Set request handler(call on every request)
/// @param[in] http http server id
/// @param[in] handler callback function
/// @param[in] param user-defined parameter
/// @return 0-ok, other-error
int http_server_set_handler(http_server_t* http, http_server_handler handler, void* param);

#ifdef __cplusplus
}
#endif
#endif /* !_http_server_h_ */
