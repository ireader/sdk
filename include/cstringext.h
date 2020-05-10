#ifndef _cstringext_h_
#define _cstringext_h_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#if defined(OS_WINDOWS)
	#if !defined(strcasecmp)
		#define strcasecmp	_stricmp
	#endif

	#if !defined(strncasecmp)
		#define strncasecmp	_strnicmp
	#endif

	#if !defined(strdup)
		#define strdup	_strdup
	#endif

	#if !defined(atoll)
		#define atoll	_atoi64
	#endif

	#if !defined(strrev)
		#define strrev _strrev
	#endif
#endif

#define strstartswith(p, prefix)	(strncmp(p, prefix, strlen(prefix))? 0 : 1)
#define strendswith(p, suffix)		(strncmp(p+strlen(p)-strlen(suffix), suffix, strlen(suffix))? 0 : 1)

#if defined(__cplusplus)
extern "C" {
#endif

/// get token(trim prefix/suffix)
/// @param[in] s source string(value will be changed '\0')
/// e.g.
/// strtoken("abc def", " ") => "abc"
/// strtoken("abc def", " c") => "ab"
char *strtoken(char *s, const char *delim, char **saveptr);

/// strsplit("ab,c, def", ",", "", &next) => 2("ab"), next="c, def"
/// strsplit("'ab,c', def", ",", "'", &next) => 6("'ab,c'"), next=" def"
/// @param[in] delimiters sperators, such as "\r\n"
/// @param[in] quotes quotes characters, such as "\'\""
/// @return token length
size_t strsplit(const char* ptr, const char* end, const char* delimiters, const char* quotes, const char** ppnext);

/// strtrim("  abc  ", &n, " ", " ") => "abc", n = 3
/// strtrim("  abc  ", &n, " ", "")  => "abc  ", n = 5
/// @param[in,out] n input string length, output string length after prefix/suffix trim
/// @param[in] prefix prefix delimiters
/// @param[in] suffix suffix delimiters
/// @return s skip prefix delimiters
const char* strtrim(const char* s, size_t* n, const char* prefix, const char* suffix);

#if defined(OS_WINDOWS)
char* strndup(const char* p, size_t n);

#if _MSC_VER < 1900 // VS2015
// the _snprintf functions do not guarantee NULL termination
int snprintf(char *str, size_t size, const char *format, ...);
#endif

#else
char* strrev(char *str);
#endif

#if !defined(OS_MAC)
size_t strlcpy(char *dst, const char *src, size_t siz);
size_t strlcat(char *dst, const char *src, size_t siz);
#endif

#if defined(__cplusplus)
}
#endif
#endif /* !_cstringext_h_ */
