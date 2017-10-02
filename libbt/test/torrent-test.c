#include "torrent.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static uint8_t s_buffer1[1024 * 1024];
static uint8_t s_buffer2[1024 * 1024];

void torrent_test(const char* file)
{
	size_t n;
	FILE* fp;
	struct torrent_t tor;

	fp = fopen(file, "rb");
	n = fread(s_buffer1, 1, sizeof(s_buffer1), fp);
	assert(n < sizeof(s_buffer1));
	fclose(fp);

	torrent_read(s_buffer1, n, &tor);
	assert(n == torrent_write(s_buffer2, sizeof(s_buffer2), &tor));
	torrent_free(&tor);

	assert(0 == memcmp(s_buffer1, s_buffer2, n));
}
