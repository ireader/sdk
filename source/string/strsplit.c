#include "cstringext.h"
#include <string.h>
#include <assert.h>

size_t strsplit(const char* ptr, const char* end, const char* delimiters, const char* quotes, const char** ppnext)
{
	char q;
	const char* p;

	assert(end && delimiters);
	q = 0;
	for (p = ptr; p && *p && p < end; p++)
	{
		if (q)
		{
			// find QUOTES first
			if (q == *p)
			{
				q = 0;
				continue;
			}
		}
		else
		{
			if (strchr(delimiters, *p))
			{
				break;
			}
			else if (quotes && strchr(quotes, *p))
			{
				q = *p;
			}
		}
	}

	if (ppnext)
	{
		*ppnext = p;
		while (*ppnext && *ppnext < end && strchr(delimiters, **ppnext))
			++* ppnext;
	}

	return p - ptr;
}

#if defined(_DEBUG) || defined(DEBUG)
void strsplit_test(void)
{
	const char* s = "abc,def, g ,h'i,', \",jk,\"";
	const char* ptr, * end, * next;
	size_t n;

	ptr = s;
	end = s + strlen(s);
	n = strsplit(ptr, end, ",", "", &next);
	assert(n == 3 && 0 == strncmp("abc", ptr, 3) && 0 == strncmp("def", next, 3));

	ptr = next;
	n = strsplit(ptr, end, ",", "'\"", &next);
	assert(n == 3 && 0 == strncmp("def", ptr, 3) && 0 == strncmp(" g ", next, 3));

	ptr = next;
	n = strsplit(ptr, end, ",", "'\"", &next);
	assert(n == 3 && 0 == strncmp(" g ", ptr, 3) && 0 == strncmp("h'i", next, 3));

	ptr = next;
	n = strsplit(ptr, end, ",", "'\"", &next);
	assert(n == 5 && 0 == strncmp("h'i,'", ptr, 5) && 0 == strncmp(" \",", next, 3));

	ptr = next;
	n = strsplit(ptr, end, ",", "'\"", &next);
	assert(n == 7 && 0 == strncmp(" \",jk,\"", ptr, 7) && next == end);
}
#endif
