#include "metainfo.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static uint8_t s_buffer1[1024 * 1024];
static uint8_t s_buffer2[1024 * 1024];

void tracker_test(const struct metainfo_t* metainfo);
void torrent_test(const struct metainfo_t* metainfo);

void metainfo_test(const char* file)
{
	size_t n;
	FILE* fp;
	uint8_t sha1[20];
	struct metainfo_t metainfo;

	fp = fopen(file, "rb");
	n = fread(s_buffer1, 1, sizeof(s_buffer1), fp);
	assert(n < sizeof(s_buffer1));
	fclose(fp);

	metainfo_read(s_buffer1, n, &metainfo);
	metainfo_hash(&metainfo, sha1);
//	tracker_test(&metainfo);
	torrent_test(&metainfo);

	assert((int)n == metainfo_write(&metainfo, s_buffer2, sizeof(s_buffer2)));
	assert(0 == memcmp(s_buffer1, s_buffer2, n));
	metainfo_free(&metainfo);
}
