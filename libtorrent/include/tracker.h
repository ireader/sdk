#ifndef _tracker_h_
#define _tracker_h_

#include "sys/sock.h"
#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define BT_PORT 6881 // 6881~6889

struct tracker_reply_t
{
	int32_t senders; // complete
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

typedef struct tracker_t tracker_t;

/// @param[in] url tracker url
/// @param[in] info_hash The 20 bytes sha1 hash of the bencoded form of the info value from the metainfo file. 
/// @param[in] peer_id A string of length 20 which this downloader uses as its id
/// @param[in] port The port number this peer is listening on
tracker_t* tracker_create(const char* url, const uint8_t info_hash[20], const uint8_t peer_id[20], uint16_t port);
void tracker_destroy(tracker_t* tracker);

/// @param[in] code 0-ok, other-error(reply invalid)
typedef void (*tracker_onquery)(void* param, int code, const struct tracker_reply_t* reply);

/// @param[in] uploaded The total amount uploaded so far
/// @param[in] downloaded The total amount downloaded so far
/// @param[in] left The number of bytes this peer still has to download
/// @param[in] event client event status
/// @return 0-ok, other-error
int tracker_query(tracker_t* tracker, uint64_t downloaded, uint64_t left, uint64_t uploaded, enum tracker_event_t event, tracker_onquery onquery, void* param);

#if defined(__cplusplus)
}
#endif
#endif /* !_tracker_h_ */
