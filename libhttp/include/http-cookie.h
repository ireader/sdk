#ifndef _http_cookie_h_
#define _http_cookie_h_

#ifdef __cplusplus
extern "C" {
#endif

typedef void* cookie_t;

/// parse HTTP cookie and create cookie object
/// @param[in] cookie HTTP Set-Cookie value
/// @param[in] bytes cookie length in byte
/// @return cookie object, NULL if failed
cookie_t http_cookie_parse(const char* cookie, size_t bytes);

void http_cookie_destroy(cookie_t cookie);

/// get cookie name
/// @param[in] cookie HTTP cookie object
/// @return cookie name, NULL if cookie invalid
const char* http_cookie_get_name(cookie_t cookie);

/// get cookie value
/// @param[in] cookie HTTP cookie object
/// @return cookie value, NULL if cookie invalid
const char* http_cookie_get_value(cookie_t cookie);

/// get cookie path attribute
/// @param[in] cookie HTTP cookie object
/// @return NULL if don't has path attribute
const char* http_cookie_get_path(cookie_t cookie);

/// get cookie domain attribute
/// @param[in] cookie HTTP cookie object
/// @return NULL if don't has domain attribute
const char* http_cookie_get_domain(cookie_t cookie);

/// get cookie expires datetime
/// @param[in] cookie HTTP cookie object
/// @return NULL if is session cookie. other return expire datetime, e.g.: Thu, 01-Jan-1970 00:00:01 GMT
const char* http_cookie_get_expires(cookie_t cookie);

/// check cookie has HttpOnly attribute
/// @param[in] cookie HTTP cookie object
/// @return 1-HttpOnly, 0-don't has HttpOnly attribute
int http_cookie_is_httponly(cookie_t cookie);

/// check cookie has Secure attribute
/// @param[in] cookie HTTP cookie object
/// @return 1-secure, 0-don't secure
int http_cookie_is_secure(cookie_t cookie);

int http_cookie_check_path(cookie_t cookie, const char* path);

int http_cookie_check_domain(cookie_t cookie, const char* domain);

/// create cookie object
int http_cookie_make(char cookie[], size_t bytes, const char* name, const char* value, const char* path, const char* domain, const char* expires, int httponly, int secure);

int http_cookie_expires(char expires[30], int hours);

#ifdef __cplusplus
}
#endif
#endif /* !_http_cookie_h_ */
