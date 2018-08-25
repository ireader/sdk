#ifndef _http_header_expires_h_
#define _http_header_expires_h_

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

int http_header_expires(const char* field, struct tm* tm);

#ifdef __cplusplus
}
#endif
#endif /* !_http_header_expires_h_ */
