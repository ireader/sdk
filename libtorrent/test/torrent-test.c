#include "torrent.h"
#include "tracker.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static uint8_t s_buffer1[1024 * 1024];
static uint8_t s_buffer2[1024 * 1024];

void torrent_test(const char* file)
{
	size_t n;
	FILE* fp;
	uint8_t sha1[20];
	struct torrent_t tor;
	struct tracker_t tracker;
	memset(&tracker, 0, sizeof(tracker));

	fp = fopen(file, "rb");
	n = fread(s_buffer1, 1, sizeof(s_buffer1), fp);
	assert(n < sizeof(s_buffer1));
	fclose(fp);

	torrent_read(s_buffer1, n, &tor);
	torrent_hash(&tor, sha1);
	//assert(0 == tracker_get("http://tracker.trackerfix.com:80/announce", sha1, "iguest", 15000, 0, 5866950861, 0, TRACKER_EVENT_STARTED, &tracker));
	assert(0 == tracker_get("udp://9.rarbg.me:2710/announce", sha1, "iguest", 15000, 0, 5866950861, 0, TRACKER_EVENT_STARTED, &tracker));
	assert(0 == tracker_get("udp://9.rarbg.to:2710/announce", sha1, "iguest", 15000, 0, 5866950861, 0, TRACKER_EVENT_STARTED, &tracker));
	assert((int)n == torrent_write(&tor, s_buffer2, sizeof(s_buffer2)));
	torrent_free(&tor);

	assert(0 == memcmp(s_buffer1, s_buffer2, n));
	tracker_free(&tracker);
}
