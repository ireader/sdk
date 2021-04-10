#include "cstringext.h"

void strtrim_test(void);
void strsplit_test(void);

void string_test(void)
{
#if !defined(NDEBUG)
	strtrim_test();
	strsplit_test();
#endif
}
