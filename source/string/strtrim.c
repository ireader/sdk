#include "cstringext.h"
#include <string.h>
#include <assert.h>

const char* strtrim(const char* s, size_t* n, const char* prefix, const char* suffix)
{
	while (s && *n > 0 && prefix && strchr(prefix, *s))
	{
		--* n;
		++s;
	}

	while (s && *n > 0 && suffix && strchr(suffix, s[*n - 1]))
		--* n;

	return s;
};

#if defined(_DEBUG) || defined(DEBUG)
void strtrim_test(void)
{
	size_t n;
	const char* ptr;

	n = 8;
	ptr = strtrim("  \tabc  ", &n, " \t", " \t");
	assert(n == 3 && 0 == strncmp("abc", ptr, 3));

	n = 8;
	ptr = strtrim("  \tabc  ", &n, " ", " ");
	assert(n == 4 && 0 == strncmp("\tabc", ptr, 4));

	n = 8;
	ptr = strtrim("  \tabc  ", &n, " ", "");
	assert(n == 6 && 0 == strncmp("\tabc  ", ptr, 6));

	n = 8;
	ptr = strtrim("  \tabc  ", &n, "", " \t");
	assert(n == 6 && 0 == strncmp("  \tabc", ptr, 6));

	n = 5;
	ptr = strtrim("  \t  ", &n, " \t", " \t");
	assert(n == 0);

	n = 5;
	ptr = strtrim("  \t  ", &n, " \t", NULL);
	assert(n == 0);

	n = 5;
	ptr = strtrim("  \t  ", &n, NULL, NULL);
	assert(n == 5);
}
#endif
