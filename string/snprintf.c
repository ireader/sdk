#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>

int snprintf(char *str, size_t size, const char *format, ...)
{
	int n = -1;
	va_list args;

	assert(size > 0);
	va_start(args, format);
	n = _vsnprintf(str, size-1, format, args);

	if(n < 0 || n == (int)size-1)
	{
		// -1 indicating that output has been truncated
		n = n < 0 ? size-1 : n;
		str[n] = '\0';
	}
	return n;
}
