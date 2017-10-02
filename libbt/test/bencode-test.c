#include "bencode.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

void bencode_test(void)
{
	int i;
	const char* s[] = {
		"d4:spaml1:a1:bli10ed1:al1:a1:beeeee",
		"d9:publisher3:bob17:publisher-webpage15:www.example.com18:publisher.location4:homee",
		"d5:filesd20:....................d8:completei5e10:downloadedi50e10:incompletei10eeee",
	};
	uint8_t buffer[1024];
	struct bvalue_t root;

	for (i = 0; i < sizeof(s) / sizeof(s[0]); i++)
	{
		bencode_read((const uint8_t*)s[i], strlen(s[i]), &root);
		bencode_write(buffer, sizeof(buffer), &root);
		assert(0 == memcmp(s[i], buffer, strlen(s[i])));
		bencode_free(&root);
	}
}