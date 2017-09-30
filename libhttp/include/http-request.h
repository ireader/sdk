#ifndef _http_request_h_
#define _http_request_h_

// HTTP version 1.0/1.1
enum { HTTP_1_0 = 0, HTTP_1_1 };
// HTTP method
enum { HTTP_GET = 0, HTTP_POST };

void* http_request_create(int version);
void http_request_destroy(void* req);
const char* http_request_get(void* req, int* bytes);

int http_request_set_uri(void* req, int method, const char* uri);
int http_request_set_host(void* req, const char* ip, int port);
int http_request_set_cookie(void* req, const char* cookie);
int http_request_set_content_lenth(void* req, unsigned int bytes);
int http_request_set_content_type(void* req, const char* value);
int http_request_set_header(void* req, const char* name, const char* value);
int http_request_set_header_int(void* req, const char* name, int value);

#endif /* !_http_request_h_ */
