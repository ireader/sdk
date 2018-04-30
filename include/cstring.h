#ifndef _cstring_h_
#define _cstring_h_

#include <string.h>

struct cstring_t
{
	const char* p;
	size_t n;
};

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
	if (0 != r)
		return r;
	return s->n > m ? 1 : 0;
}

// cstrcmp("abc", "ab") => 1
static inline int cstrcmp(const struct cstring_t* s, const char* c)
{
	int r;
	size_t n;
	n = strlen(c);
	r = cstrncmp(s, c, n);
	return 0 != r ? r : (s->n > n ? 1 : 0);
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
