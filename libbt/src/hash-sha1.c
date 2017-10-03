#include "sha.h"

int hash_sha1(const uint8_t* data, size_t bytes, uint8_t sha1[20])
{
	SHA1Context ctx;
	SHA1Reset(&ctx);
	SHA1Input(&ctx, data, bytes);
	SHA1Result(&ctx, sha1);
	return 0;
}
