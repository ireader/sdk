#ifndef _tracker_h_
#define _tracker_h_

#include "sys/sock.h"
#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define BT_PORT 6881 // 6881~6889

struct tracker_t
{
	char* errmsg; // NULL if ok

	//int32_t complete;
	//int32_t incomplete;
	int32_t seeders; // complete
	int32_t leechers; // incomplete

	int32_t interval;

	struct sockaddr_storage* peers;
	size_t peer_count;
};

enum tracker_event_t
{
	TRACKER_EVENT_EMPTY = 0,
	TRACKER_EVENT_COMPLETED,
	TRACKER_EVENT_STARTED,
	TRACKER_EVENT_STOPPED,
};

/// @param[in] url tracker url
/// @param[in] info_hash The 20 byte sha1 hash of the bencoded form of the info value from the metainfo file. 
/// @param[in] peer_id A string of length 20 which this downloader uses as its id
/// @param[in] port The port number this peer is listening on
/// @param[in] uploaded The total amount uploaded so far
/// @param[in] downloaded The total amount downloaded so far
/// @param[in] left The number of bytes this peer still has to download
/// @param[in] event client event status
/// @return 0-ok, other-error
int tracker_get(const char* url, 
	const uint8_t info_hash[20],
	const char* usr,
	int port, 
	uint64_t downloaded,
	uint64_t left,
	uint64_t uploaded,
	enum tracker_event_t event,
	struct tracker_t* tracker);

int tracker_free(struct tracker_t* tracker);

#if defined(__cplusplus)
}
#endif
#endif /* !_tracker_h_ */
