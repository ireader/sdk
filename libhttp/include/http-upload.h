#ifndef _http_upload_h_
#define _http_upload_h_

#include <stddef.h>

#ifdef  __cplusplus
extern "C" {
#endif

/// @return 0-ok, other-error
int http_get_upload_boundary(const char* contentType, char boundary[128]);

typedef void (*on_http_upload_data)(void* param, const char* filed, const void* data, size_t size);

/// @return 0-ok, other-error
int http_get_upload_data(const void* data, unsigned int size, const char* boundary, on_http_upload_data ondata, void* cbparam);

#ifdef  __cplusplus
}
#endif

#endif /* !_http_upload_h_ */
