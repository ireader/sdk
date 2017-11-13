// https://en.wikipedia.org/wiki/Magnet_URI_scheme
// magnet:?xt=urn:ed2k:31D6CFE0D16AE931B73C59D7E0C089C0&xl=0&dn=zero_len.fil&xt=urn:bitprint:3I42H3S6NNFQ2MSVX7XZKYAYSCX5QBYJ.LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ&xt=urn:md5:D41D8CD98F00B204E9800998ECF8427E
// magnet:?xt=urn:ed2k:354B15E68FB8F36D7CD88FF94116CDC1&xt=urn:tree:tiger:7N5OAMRNGMSSEUE3ORHOKWN4WWIQ5X4EBOOTLJY&xt=urn:btih:QHQXPYWMACKDWKP47RRVIV7VOURXFE5Q&xl=10826029&dn=mediawiki-1.15.1.tar.gz&tr=udp%3A%2F%2Ftracker.openbittorrent.com%3A80%2Fannounce&as=http%3A%2F%2Fdownload.wikimedia.org%2Fmediawiki%2F1.15%2Fmediawiki-1.15.1.tar.gz&xs=http%3A%2F%2Fcache.example.org%2FXRX2PEFXOOEJFRVUCX6HMZMKS5TWG4K5&xs=dchub://example.org

#include "magnet.h"
#include "base64.h"
#include "urlcodec.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

static int manget_parse_xt(struct magnet_t* magnet, const char* url)
{
	int i;
	const char* p;
	p = strchr(url, '=');
	if (!p || 0 != strncmp("=urn:", p, 5))
		return -1;

	p += 5; // skip =urn:
	if (0 == strncmp("btih:", p, 5))
	{
		magnet->protocol = MAGNET_HASH_BT;
		for (i = 5; i < 45; i++)
		{
			if (!isalnum(p[i]))
				return -1;
		}

		base16_decode(magnet->info_hash, p + 5, 40);
	}
	else if (0 == strncmp("ed2k:", p, 5))
	{
		magnet->protocol = MAGNET_HASH_ED2K;
	}
	else
	{
		// TODO
	}
	return 0;
}

static int manget_parse_tr(struct magnet_t* magnet, const char* url, size_t len)
{
	const char* p;
	p = strchr(url, '=');
	if (!p)
		return -1;

	if (magnet->tracker_count > sizeof(magnet->trackers) / sizeof(magnet->trackers[0]))
		return -1;

	magnet->trackers[magnet->tracker_count] = malloc(len + 1);
	if (!magnet->trackers[magnet->tracker_count])
		return -1; // ENOMEM

	url_decode(p + 1, len - (p + 1 - url), magnet->trackers[magnet->tracker_count], len + 1);
	magnet->trackers[magnet->tracker_count++][len] = 0;
	return 0;
}

struct magnet_t* magnet_parse(const char* url)
{
	int r;
	size_t len;
	const char* p, *next;
	struct magnet_t* magnet;

	if (0 != strncmp("magnet:?", url, 8))
		return NULL;

	magnet = calloc(1, sizeof(*magnet));
	if (!magnet)
		return NULL;

	r = 0;
	for (p = url + 8 - 1; 0 == r && p++; p = next)
	{
		next = strchr(p, '&');
		len = next ? next - p : strlen(p);

		if (0 == strncmp("xt", p, 2))
		{
			r = manget_parse_xt(magnet, p);
		}
		else if (0 == strncmp("tr", p, 2))
		{
			r = manget_parse_tr(magnet, p, len);
		}
		else if (0 == strncmp("xl", p, 2))
		{
		}
		else
		{
			// TODO:
		}
	}

	if (0 != r)
	{
		free(magnet);
		magnet = NULL;
	}
	return magnet;
}

void magnet_free(struct magnet_t* magnet)
{
	if (!magnet)
		return;

	assert(magnet->tracker_count < sizeof(magnet->trackers) / sizeof(magnet->trackers[0]));
	while (magnet->tracker_count > 0)
	{
		free(magnet->trackers[--magnet->tracker_count]);
	}
	free(magnet);
}

#if defined(_DEBUG) || defined(DEBUG)
void magnet_test(void)
{
	const uint8_t info_hash[] = { 0xde, 0xc8, 0xae, 0x69, 0x73, 0x51, 0xff, 0x4a, 0xec, 0x29, 0xcd, 0xba, 0xab, 0xf2, 0xfb, 0xe3, 0x46, 0x7c, 0xc2, 0x67 };
	const char* url = "magnet:?xt=urn:ed2k:354B15E68FB8F36D7CD88FF94116CDC1&xt=urn:tree:tiger:7N5OAMRNGMSSEUE3ORHOKWN4WWIQ5X4EBOOTLJY&xt=urn:btih:dec8ae697351ff4aec29cdbaabf2fbe3467cc267&xl=10826029&dn=mediawiki-1.15.1.tar.gz&tr=udp%3A%2F%2Ftracker.openbittorrent.com%3A80%2Fannounce&as=http%3A%2F%2Fdownload.wikimedia.org%2Fmediawiki%2F1.15%2Fmediawiki-1.15.1.tar.gz&xs=http%3A%2F%2Fcache.example.org%2FXRX2PEFXOOEJFRVUCX6HMZMKS5TWG4K5&xs=dchub://example.org";
	struct magnet_t* magnet;

	magnet = magnet_parse(url);
	assert(MAGNET_HASH_BT == magnet->protocol);
	assert(0 == memcmp(magnet->info_hash, info_hash, sizeof(magnet->info_hash)));
	assert(1 == magnet->tracker_count);
	assert(0 == strcmp(magnet->trackers[0], "udp://tracker.openbittorrent.com:80/announce"));
	free(magnet);
}
#endif
