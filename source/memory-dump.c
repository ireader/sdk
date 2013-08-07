#include <stdio.h>

static void memory_dump(const void* ptr, unsigned int len)
{
	unsigned int i, j;
	const unsigned char *p;

	p = (const unsigned char*)ptr;
	for(j = 0; j < len; j += 16)
	{
		for(i=0; i < 16; i++)
			printf("%02x ", i+j<len ? (unsigned int)p[i+j] : 0);

		for(i=0; i < 16; i++)
			printf("%c", i+j<len ? p[i+j] : ' ');

		printf("\n");
	}
}
