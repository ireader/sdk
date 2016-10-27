#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>

#if defined(_MSC_VER) && (_MSC_VER < 1900)
int snprintf(char *str, size_t size, const char *format, ...)
{
	int n = -1;
	va_list args;

	if (size < 1)
		return 0;

	va_start(args, format);
	n = _vsnprintf(str, size-1, format, args);
	va_end(args);

	if(n == (int)size-1)
		str[n] = '\0';
	return n;
}
#endif
