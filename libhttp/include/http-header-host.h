#ifndef _http_header_host_h_
#define _http_header_host_h_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/// parse HTTP HOST header
/// @param[in] field host value(don't include "HOST: " and "\r\n")
/// @param[out] host ip or domain
/// @param[in] bytes host buffer length in byte
/// @param[out] port host port value(don't change if field don't have port)
/// @return 0-ok, other-error
/// e.g. 
/// 1. http_header_host("www.baidu.com") => ("wwww.baidu.com", undefined port value)
/// 2. http_header_host("www.baidu.com:80") => ("wwww.baidu.com", 80)
/// 3. http_header_host("192.168.1.100:8081") => ("192.168.1.100", 8081)
int http_header_host(const char* field, char host[], size_t bytes, unsigned short *port);

#ifdef __cplusplus
}
#endif
#endif /* !_http_header_host_h_ */
