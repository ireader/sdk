#ifndef _magnet_h_
#define _magnet_h_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum manget_hash_t
{
	MAGNET_HASH_BT       = 0x01, // xt=urn:btih:[ BitTorrent Info Hash (Hex) ]
	MAGNET_HASH_TTH      = 0x02, // xt=urn:tree:tiger:[ TTH Hash (Base32) ]
	MAGNET_HASH_SHA1     = 0x04, // xt=urn:sha1:[ SHA-1 Hash (Base32) ]
	MAGNET_HASH_BITPRINT = 0x08, // xt=urn:bitprint:[ SHA-1 Hash (Base32) ].[ TTH Hash (Base32) ]
	MAGNET_HASH_ED2K     = 0x10, // xt=urn:ed2k:[ ED2K Hash (Hex) ]
	MAGNET_HASH_AICH     = 0x20, // xt=urn:aich:[ aich Hash (Base32) ]
	MAGNET_HASH_KAZAA    = 0x40, // xt=urn:kzhash:[ Kazaa Hash (Hex) ]
	MAGNET_HASH_MD5      = 0x80, // xt=urn:md5:[MD5 Hash(Hex)]
};

struct magnet_t
{
	int protocol; // MAGNET_HASH_XXX bit or
	uint8_t info_hash[20];

	char* trackers[16]; // tr: Tracker URL for BitTorrent downloads
	unsigned int tracker_count;
};

struct magnet_t* magnet_parse(const char* url);

void magnet_free(struct magnet_t* magnet);

#if defined(__cplusplus)
}
#endif
#endif /* !_magnet_h_ */
