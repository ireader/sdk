#ifndef _url_h_
#define _url_h_

#include <stdlib.h>

#ifdef  __cplusplus
extern "C" {
#endif

// http://www.microsoft.com:80/windows/index.aspx
// scheme: http
// host: www.microsoft.com
// port: 80
// path: /windows/index.aspx

void* url_new();
void* url_parse(const char* url);

void url_free(void* id);

//int url_geturl(void* id, char* url, size_t len);
//int url_geturlpath(void* id, char* url, size_t len);

//int url_sethost(void* id, const char* host);
const char* url_gethost(void* id);

//int url_setport(void* id, int port);
int url_getport(void* id);

//int url_setscheme(void* id, const char* scheme);
const char* url_getscheme(void* id);

//int url_setpath(void* id, const char* path);
const char* url_getpath(void* id);

//int url_setparam(void* id, const char* name, const char* value);
int url_getparam_count(void* id);
int url_getparam(void* id, int index, const char** name, const char** value);

#ifdef  __cplusplus
}
#endif

#endif /* !_url_h_ */
