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
	MAGNET_HASH_BT_V2	 = 0x100, // xt=urn:btmh:[ BitTorrent Info Hash (Hex) ]
};

struct magnet_t
{
	int protocol; // MAGNET_HASH_XXX bit or
	uint8_t bt_hash[20];
	//uint8_t btv2_hash[64];
	uint8_t tth_hash[20];
	//uint8_t sha1_hash[20];
	//uint8_t bitprint_hash[20];
	uint8_t ed2k_hash[16];
	//uint8_t aich_hash[20];
	//uint8_t kazaa_hash[20];
	//uint8_t md5_hash[20];

	uint64_t size; // 0-unknown
	char *name;
	char *keywords;

	char* trackers[16]; // tr: Tracker URL for BitTorrent downloads
	int tracker_count;
	char* xs[16];
	int xs_count;
	char* as[16];
	int as_count;

	size_t __off; // internal use only
};

struct magnet_t* magnet_parse(const char* url);

void magnet_free(struct magnet_t* magnet);

#if defined(__cplusplus)
}
#endif
#endif /* !_magnet_h_ */
