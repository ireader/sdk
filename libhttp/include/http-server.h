#ifndef _http_server_h_
#define _http_server_h_

#include "http-websocket.h"

// HTTP Response
// 1. [OPTIONAL] use http_server_set_status_code to set status code(default 200-OK)
// 2. [OPTIONAL] use http_server_set_content_length to set content-lenght(default 0)
// 3. [OPTIONAL] use http_server_set_content_type to set content-type(default don't set)
// 4. [OPTIONAL] use http_server_set_header/http_server_set_header_int to set other header
// 5. [MUST] use http_server_send/http_server_send_vec to send response

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_server_t http_server_t;
typedef struct http_session_t http_session_t;

/// @param[in] code HTTP status-code(200-OK, 301-Move Permanently, ...)
/// @return 0-ok(goto next session), 1-more data to send, other-close socket
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

/// Set HTTP response status code, default 200-OK
/// @param[in] code HTTP status-code(200-OK, 301-Move Permanently, ...)
/// @return 0-ok, other-error
int http_server_set_status_code(http_session_t* session, int code, const char* status);

/// Set response http header Content-Length field value(every reply must set it again), default 0
/// @param[in] session handle callback session parameter
/// @param[in] value content-length value, -1-don't auto set Content-Length header
/// @return 0-ok, other-error
int http_server_set_content_length(http_session_t* session, int64_t value);

/// Set response http header Content-Type field value(every reply must set it again)
/// @param[in] session handle callback session parameter
/// @param[in] value content-type value
/// @return 0-ok, other-error
int http_server_set_content_type(http_session_t* session, const char* value);

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

/// Response Content(1 or more times)
/// @param[in] session handle callback session parameter
/// @return 0-ok, other-error
int http_server_send(http_session_t* session, const void* data, size_t bytes, http_server_onsend onsend, void* param);

/// Response Content(1 or more times)
/// @param[in] session handle callback session parameter
/// @param[in] vec bundle array
/// @param[in] num array elementary number
/// @return 0-ok, other-error
int http_server_send_vec(http_session_t* session, const struct http_vec_t* vec, int num, http_server_onsend onsend, void* param);

/// Reply a server side file(must be local regular file)
/// @param[in] session handle callback session parameter
/// @param[in] localpath local regular file pathname
/// @return 0-ok, other-error
int http_server_sendfile(http_session_t* session, const char* localpath, http_server_onsend onsend, void* param);


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

/// WebSocket
void http_server_websocket_sethandler(http_server_t* http, const struct websocket_handler_t* handler, void* param);


/// HTTP session handler, for upload/post big data (>16K)
struct http_streaming_handler_t
{
	/// HTTP session destroy notify
	void (*ondestroy)(void* param);

	/// HTTP session received data
	/// @param[in] code 0-OK, other-error
	/// @param[in] data payload data, upload file content or post data
	/// @param[in] >0-data length, =0-peer close connection
	void (*onrecv)(void* param, int code, const void* data, size_t bytes);
};

/// Set http streaming handler(for upload / flv streaming)
void http_session_streaming_handler(http_session_t* session, struct http_streaming_handler_t* handler, void* param);


/// Close HTTP session(don't need do this in simple mode)
int http_session_close(http_session_t* session);

#ifdef __cplusplus
}
#endif
#endif /* !_http_server_h_ */
