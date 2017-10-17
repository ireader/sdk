#include "metainfo.h"
#include "tracker.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static uint8_t s_buffer1[1024 * 1024];
static uint8_t s_buffer2[1024 * 1024];

static void tracker_onreply(void* param, int code, const struct tracker_reply_t* reply)
{
	struct tracker_t* tracker;
	tracker = (struct tracker_t*)param;
	tracker_destroy(tracker);
}

void torrent_test(const char* file)
{
	size_t n;
	FILE* fp;
	uint8_t sha1[20];
	struct tracker_t* tracker;
	struct metainfo_t metainfo;
	memset(&tracker, 0, sizeof(tracker));

	fp = fopen(file, "rb");
	n = fread(s_buffer1, 1, sizeof(s_buffer1), fp);
	assert(n < sizeof(s_buffer1));
	fclose(fp);

	metainfo_read(s_buffer1, n, &metainfo);
	metainfo_hash(&metainfo, sha1);
	assert((int)n == metainfo_write(&metainfo, s_buffer2, sizeof(s_buffer2)));
	assert(0 == memcmp(s_buffer1, s_buffer2, n));
	metainfo_free(&metainfo);

	tracker = tracker_create("http://tracker.trackerfix.com:80/announce", sha1, "iguest", 15000);
	//tracker = tracker_create("udp://9.rarbg.me:2710/announce", sha1, "iguest", 15000);
	//tracker = tracker_create("udp://9.rarbg.to:2710/announce", sha1, "iguest", 15000);
	assert(0 == tracker_query(tracker, 0, 5866950861, 0, TRACKER_EVENT_STARTED, tracker_onreply, tracker));
}
