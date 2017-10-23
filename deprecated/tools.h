#ifndef _tools_h_
#define _tools_h_

#include <stdarg.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef int (*tools_tokenline_fcb)(const char* str, int len, va_list val);

int tools_tokenline(const char* str, tools_tokenline_fcb fcb, ...);

// tools_cat("/root/config") => "..."
int tools_cat(const char* filename, char* buf, int len);
int tools_cat_binary(const char* filename, char* buf, int len);

int tools_write(const char* filename, const void* ptr, int len);

int tools_append(const char* filename, const void* ptr, int len);

// tools_grep("abc\ndef\ncf", "c") => "abc\ncf"
int tools_grep(const char* in, const char* pattern, char* buf, int len);

#ifdef  __cplusplus
}
#endif

#endif /* !_tools_h_ */
