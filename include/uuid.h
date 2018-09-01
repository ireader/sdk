#ifndef _uuid_h_
#define _uuid_h_

#include <stdio.h>
#if defined(OS_WINDOWS)
#include <combaseapi.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

// UUID: 00000000-0000-0000-0000-000000000000

#if defined(OS_WINDOWS)
static inline void uuid_generate(char s[37])
{
	GUID guid;
	CoCreateGuid(&guid);

	snprintf(s, 36, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		guid.Data1, guid.Data2, guid.Data3, (unsigned int)guid.Data4[0], (unsigned int)guid.Data4[1],
		(unsigned int)guid.Data4[2], (unsigned int)guid.Data4[3], (unsigned int)guid.Data4[4],
		(unsigned int)guid.Data4[5], (unsigned int)guid.Data4[6], (unsigned int)guid.Data4[7]);
}

#else

void uuid_generate_simple(char s[37]);

static inline void uuid_generate(char s[37])
{
	FILE* fp;
	fp = fopen("/proc/sys/kernel/random/uuid", "r");
	if (fp)
	{
		s[36] = '0';
		fread(s, 1, 36, fp);
		fclose(fp);
	}
	else
	{
		uuid_generate_simple(s);
	}
}
#endif

#if defined(__cplusplus)
}
#endif
#endif /* !_uuid_h_ */
