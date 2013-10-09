#ifndef _cstringext_h_
#define _cstringext_h_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
	#ifndef OS_WINDOWS
		#define OS_WINDOWS
	#endif
#endif

#if defined(OS_WINDOWS)
	#if !defined(__cplusplus)
		#ifndef inline
			#define inline __inline
		#endif
	#endif

	#if !defined(stricmp)
		#define stricmp		_stricmp
	#endif

	#if !defined(strnicmp)
		#define strnicmp	_strnicmp
	#endif

	#if !defined(strdup)
		#define strdup		_strdup
	#endif

	#if !defined(snprintf)
		#define snprintf	_snprintf
	#endif

	#if !defined(atoll)
		#define atoll	_atoi64
	#endif
#else
	#if !defined(__cplusplus)
		#define inline static __attribute__((unused))
	#endif

	#ifndef stricmp
		#define stricmp strcasecmp
	#endif

	#ifndef strnicmp
		#define strnicmp strncasecmp
	#endif

	#ifndef _snprintf
		#define _snprintf snprintf
	#endif
#endif

#define char_isnum(c)				('0'<=(c) && '9'>=(c))
#define char_isalpha(c)				(('a'<=(c) && 'z'>=(c)) || ('A'<=(c) && 'Z'>=(c)))
#define char_isalnum(c)				(char_isnum(c) || char_isalpha(c))

#define strempty(s)					((!s || 0==*s) ? 1 : 0)
#define streq(s, t)					(strcmp(s, t) ? 0 : 1)
#define strieq(s, t)				(stricmp(s, t) ? 0 : 1)
#define strneq(s, t, n)				(strncmp(s, t, n) ? 0 : 1)
#define strnieq(s, t, n)			(strnicmp(s, t, n) ? 0 : 1)
#define strstartswith(p, prefix)	(strnicmp(p, prefix, strlen(prefix))? 0 : 1)

#define STRLEN(s)					(s?strlen(s):0)
#define FREE(p)						do{if(p){free(p); p=NULL;}}while(p)

#ifndef MAX
#define MAX(a, b)					((a)<(b)?(b):(a))
#endif

#ifndef MIN
#define MIN(a, b)					((a)<(b)?(a):(b))
#endif

#ifndef MAX_PATH
#define MAX_PATH					260
#endif

inline const char* skip(const char* p, char c)
{
	while(p && *p==c)
		++p;
	return p;
}

inline const char* skips(const char* p, const char* chars)
{
	while(p && strchr(chars, *p))
		++p;
	return p;
}

/// token("abc def", " ") => "abc"
/// token("abc def", " c") => "ab"
inline int token(const char* p, const char* chars, char* buf, int len)
{
	int i = 0;
	while(p && *p && !strchr(chars, *p) && i<len-1)
	{
		buf[i++] = *p;
		++p;
	}
	buf[i] = 0;
	return i;
}

#if defined(OS_WINDOWS)
inline char* strndup(const char* p, size_t n)
{
	char* s;
	n = n?n:strlen(p);

	s = (char*)malloc(n+1);
	if(!s)
		return NULL;

	strncpy(s, p, n);
	s[n] = 0;
	return s;
}

#else

#ifndef GetLastError
	#define GetLastError() errno
#endif

#endif

#endif /* !_cstringext_h_ */
