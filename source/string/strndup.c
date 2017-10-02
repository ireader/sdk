#include <stdlib.h>
#include <string.h>

char* strndup(const char* p, size_t n)
{
	char* s;
	n = strnlen(p, n);

	s = (char*)malloc(n+1);
	if(!s)
		return NULL;

	memcpy(s, p, n);
	s[n] = 0;
	return s;
}
