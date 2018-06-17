#ifndef _cstring_h_
#define _cstring_h_

#include <string.h>
#include <stdlib.h>

struct cstring_t
{
	const char* p;
	size_t n;
};

static inline int cstrvalid(const struct cstring_t* s)
{
	return (s->p && s->n) > 0 ? 1 : 0;
}

// same as strchr
static inline const char* cstrchr(const struct cstring_t* s, int c)
{
	const char* p;
	p = strchr(s->p, c);
	return p && p < s->p + s->n ? p : NULL;
}

// same as strpbrk
static inline const char* cstrpbrk(const struct cstring_t* s, const char* delimiters)
{
	const char* p;
	p = strpbrk(s->p, delimiters);
	return p && p < s->p + s->n ? p : NULL;
}

// same as strlcpy
static inline size_t cstrcpy(const struct cstring_t* s, char* dst, size_t size)
{
	size_t n;
	n = s->n > size ? size : s->n;
	memcpy(dst, s->p, n);

	if(n < size)
		dst[n] = 0; // fill with '\0'

	return s->n;
}

// cstrltrim("  abc  ") => "abc  "
static inline void cstrltrim(struct cstring_t* s, const char* trims)
{
	while (s->n > 0 && strchr(trims, s->p[0]))
	{
		--s->n;
		++s->p;
	}
}

// cstrrtrim("  abc  ") => "  abc"
static inline void cstrrtrim(struct cstring_t* s, const char* trims)
{
	while (s->n > 0 && strchr(trims, s->p[s->n - 1]))
		--s->n;
}

// cstrtrim("  abc  ") => "abc"
static inline void cstrtrim(struct cstring_t* s, const char* trims)
{
	cstrltrim(s, trims);
	cstrrtrim(s, trims);
}

// cstrquoted("\"abc\"") => "abc"
static inline void cstrquoted(struct cstring_t* s)
{
	if (s->n > 1 && s->p[0] == '"' && s->p[s->n - 1] == '"')
	{
		s->n -= 2;
		s->p += 1;
	}
}

// cstrncmp("abc", "ab", 2) => 0
static inline int cstrncmp(const struct cstring_t* s, const char* c, size_t n)
{
	int r;
	size_t m;

	m = s->n > n ? n : s->n;
	r = memcmp(s->p, c, m);
	return 0 != r ? r : (s->n > n ? 1 : -1);
}

// cstrcmp("abc", "ab") => 1
static inline int cstrcmp(const struct cstring_t* s, const char* c)
{
	return cstrncmp(s, c, strlen(c));
}

// same as strnicmp/strncasecmp
static inline int cstrncasecmp(const struct cstring_t* s, const char* c, size_t n)
{
	int r;
	size_t m;

	m = s->n > n ? n : s->n;
#if defined(OS_WINDOWS)
	r = strnicmp(s->p, c, m);
#else
	r = strcasecmp(s->p, c, m);
#endif
	return 0 != r ? r : (s->n > n ? 1 : -1);
}

// same as stricmp/strcasecmp
static inline int cstrcasecmp(const struct cstring_t* s, const char* c)
{
	return cstrncasecmp(s, c, strlen(c));
}

// cstrprefix("abc", "ab") => 1
static inline int cstrprefix(const struct cstring_t* s, const char* prefix)
{
	return cstrncmp(s, prefix, strlen(prefix));
}

// cstrsuffix("abc", "bc") => 1
static inline int cstrsuffix(const struct cstring_t* s, const char* suffix)
{
	size_t n;
	n = strlen(suffix);
	if (n > s->n)
		return -1;
	return memcmp(s->p + s->n - n, suffix, n);
}

#endif /* !_cstring_h_ */
