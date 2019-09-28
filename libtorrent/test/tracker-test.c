#include "tracker.h"
#include "torrent.h"
#include "metainfo.h"
#include "aio-worker.h"
#include "sys/system.h"
#include <assert.h>

static void tracker_onreply(void* param, int code, const struct tracker_reply_t* reply)
{
	struct tracker_t* tracker;
	tracker = (struct tracker_t*)param;
	printf("tracker: %p => %d\n", tracker, code);
	tracker_destroy(tracker);
}

void tracker_test(const struct metainfo_t* metainfo)
{
	size_t i;
	uint8_t id[20];
	uint8_t sha1[20];
	struct tracker_t* tracker;

	aio_worker_init(4);
	metainfo_hash(metainfo, sha1);
	torrent_peer_id("libtorrent", id);
	for (i = 0; i < metainfo->tracker_count; i++)
	{
		tracker = tracker_create(metainfo->trackers[i], sha1, id, 15000);
		printf("tracker: %p, %s\n", tracker, metainfo->trackers[i]);
		assert(0 == tracker_query(tracker, 0, metainfo->piece_bytes * metainfo->piece_count, 0, TRACKER_EVENT_STARTED, tracker_onreply, tracker));
	}

	system_sleep(10000);
	aio_worker_clean(4);
}
