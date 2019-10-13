// https://en.wikipedia.org/wiki/Magnet_URI_scheme
// magnet:?xt=urn:ed2k:31D6CFE0D16AE931B73C59D7E0C089C0&xl=0&dn=zero_len.fil&xt=urn:bitprint:3I42H3S6NNFQ2MSVX7XZKYAYSCX5QBYJ.LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ&xt=urn:md5:D41D8CD98F00B204E9800998ECF8427E
// magnet:?xt=urn:ed2k:354B15E68FB8F36D7CD88FF94116CDC1&xt=urn:tree:tiger:7N5OAMRNGMSSEUE3ORHOKWN4WWIQ5X4EBOOTLJY&xt=urn:btih:QHQXPYWMACKDWKP47RRVIV7VOURXFE5Q&xl=10826029&dn=mediawiki-1.15.1.tar.gz&tr=udp%3A%2F%2Ftracker.openbittorrent.com%3A80%2Fannounce&as=http%3A%2F%2Fdownload.wikimedia.org%2Fmediawiki%2F1.15%2Fmediawiki-1.15.1.tar.gz&xs=http%3A%2F%2Fcache.example.org%2FXRX2PEFXOOEJFRVUCX6HMZMKS5TWG4K5&xs=dchub://example.org

// http://www.bittorrent.org/beps/bep_0009.html
// v1: magnet:?xt=urn:btih:<info-hash>&dn=<name>&tr=<tracker-url>&x.pe=<peer-address>
// v2: magnet:?xt=urn:btmh:<tagged-info-hash>&dn=<name>&tr=<tracker-url>&x.pe=<peer-address>

// Multiple files and their URNs, names and hashes in the Magnet link can be included by adding a count number preceded by a dot (".") to each link parameter.
// magnet:?xt.1=[ URN of the first file]&xt.2=[ URN of the second file]

#include "magnet.h"
#include "base64.h"
#include "urlcodec.h"
#include "uri-parse.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <ctype.h>

#define N_MAGNET 2048

static char* magnet_copy(struct magnet_t* magnet, const char* str, size_t len)
{
	char* p;
	if (len + magnet->__off > N_MAGNET)
		return NULL;

	p = (char*)(magnet + 1) + magnet->__off;
	magnet->__off += snprintf(p, N_MAGNET - magnet->__off, "%.*s", len, str) + 1;
	return p;
}

static char* magnet_url_copy(struct magnet_t* magnet, const char* url, size_t len)
{
	char* p;
	p = (char*)(magnet + 1) + magnet->__off;
	if (0 != url_decode(url, len, p, N_MAGNET - magnet->__off))
		return NULL;

	magnet->__off += strlen(p) + 1;
	return p;
}

static int manget_parse_xt(struct magnet_t* magnet, const char* url, int len)
{
	int n;
	const char* p;
	if (len < 5 || 0 != strncmp("urn:", url, 4))
		return -1;

	len -= 4;
	p = url + 4; // skip urn:
	if (len > 5 && 0 == strncmp("btih:", p, 5))
	{
		// These are hex-encoded SHA-1 hash sums of the "info" sections of BitTorrent metafiles as used by BitTorrent to identify downloadable files or sets of files. For backwards compatibility with existing links, clients should also support the Base32 encoded version of the hash.[1]
		// xt=urn:btih:[ BitTorrent Info Hash (Hex) ]
		if (len == 5 + 32)
		{
			n = base32_decode(magnet->bt_hash, p + 5, 32);
		}
		else if (len == 5 + 40)
		{
			n = base16_decode(magnet->bt_hash, p + 5, 40);
		}
		else
		{
			n = 0;
		}

		if (20 != n)
			return -1;

		magnet->protocol |= MAGNET_HASH_BT;
	}
	else if (len > 7 && 0 == strncmp("btmh:", p, 5))
	{
		// http://www.bittorrent.org/beps/bep_0009.html
		// v1: magnet:?xt=urn:btih:<info-hash>&dn=<name>&tr=<tracker-url>&x.pe=<peer-address>
		// v2: magnet:?xt=urn:btmh:<tagged-info-hash>&dn=<name>&tr=<tracker-url>&x.pe=<peer-address>

		// https://multiformats.io/multihash/
		// https://github.com/multiformats/multicodec/blob/master/table.csv
		//magnet->protocol |= MAGNET_HASH_BT;
		assert(0);
	}
	else if (0 == strncmp("ed2k:", p, 5))
	{
		// eDonkey2000.
		// xt=urn:ed2k:[ ED2K Hash (Hex) ]
		if (len == 5 + 32)
		{
			base16_decode(magnet->ed2k_hash, p + 5, 32);
			magnet->protocol |= MAGNET_HASH_ED2K;
		}
		else
		{
			assert(0);
		}
	}
	else if (0 == strncmp("tree:tiger:", p, 11))
	{
		// Direct Connect and G2 (Gnutella2)
		// xt=urn:tree:tiger:[ TTH Hash (Base32) ]
		if (len == 11 + 39)
		{
			// https://en.wikipedia.org/wiki/Merkle_tree
			// https://en.wikipedia.org/wiki/Tiger_(hash_function)
			n = base32_decode(magnet->tth_hash, p + 11, 39);
			magnet->protocol |= MAGNET_HASH_TTH;
			assert(n == 20);
		}
		else
		{
			assert(0);
		}
	}
	else if (0 == strncmp("sha1:", p, 5))
	{
		// gnutella and G2 (Gnutella2)
		// xt=urn:sha1:[ SHA-1 Hash (Base32) ]
		//magnet->protocol |= MAGNET_HASH_SHA1;
	}
	else if (0 == strncmp("bitprint:", p, 9))
	{
		// Such hash sums consist of an SHA-1 Hash, followed by a TTH Hash, delimited by a point; they are used on gnutella and G2 (Gnutella2).
		// xt=urn:bitprint:[ SHA-1 Hash (Base32) ].[ TTH Hash (Base32) ]
		//magnet->protocol |= MAGNET_HASH_BITPRINT;
	}
	else if (0 == strncmp("kzhash:", p, 7))
	{
		// Used on FastTrack, these hash sums are vulnerable to hash collision attacks.
		// xt=urn:kzhash:[ Kazaa Hash (Hex) ]
		//magnet->protocol |= MAGNET_HASH_BITPRINT;
	}
	else if (0 == strncmp("md5:", p, 4))
	{
		// Supported by G2 (Gnutella2), such hashes are vulnerable to hash collision attacks.
		// xt=urn:md5:[ MD5 Hash (Hex) ]
	}
	else
	{
		// TODO
		assert(0);
	}
	return 0;
}

static int manget_add_tr(struct magnet_t* magnet, const char* url, size_t len)
{
	char* p;
	if (magnet->tracker_count > sizeof(magnet->trackers) / sizeof(magnet->trackers[0]))
		return -1;

	p = magnet_url_copy(magnet, url, len);
	if (!p)
		return -1;

	magnet->trackers[magnet->tracker_count++] = p;
	return 0;
}

static int manget_add_as(struct magnet_t* magnet, const char* url, size_t len)
{
	char* p;
	if (magnet->as_count > sizeof(magnet->as) / sizeof(magnet->as[0]))
		return -1;

	p = magnet_url_copy(magnet, url, len);
	if (!p)
		return -1;

	magnet->as[magnet->as_count++] = p;
	return 0;
}

static int manget_add_xs(struct magnet_t* magnet, const char* url, size_t len)
{
	char* p;
	if (magnet->xs_count > sizeof(magnet->xs) / sizeof(magnet->xs[0]))
		return -1;

	p = magnet_url_copy(magnet, url, len);
	if (!p)
		return -1;

	magnet->xs[magnet->xs_count++] = p;
	return 0;
}

struct magnet_t* magnet_parse(const char* url)
{
	int i, r, n;
	struct magnet_t* magnet;
	struct uri_query_t* query;

	if (0 != strncmp("magnet:?", url, 8))
		return NULL;

	magnet = malloc(sizeof(*magnet) + N_MAGNET);
	if (!magnet)
		return NULL;
	memset(magnet, 0, sizeof(struct magnet_t));

	n = uri_query(url + 8, url + strlen(url), &query);
	for (r = i = 0; i < n && 0 == r; i++)
	{
		if ( (query[i].n_name == 2 && 0 == strncmp("xt", query[i].name, 2)) || (query[i].n_name >= 3 && 0 == strncmp("xt.", query[i].name, 3)))
		{
			// (eXact Topic): URN containing file hash
			// match xt/xt.xxx
			r = manget_parse_xt(magnet, query[i].value, query[i].n_value);
		}
		else if ((query[i].n_name == 2 && 0 == strncmp("tr", query[i].name, 2)) || (query[i].n_name >= 3 && 0 == strncmp("tr.", query[i].name, 3)))
		{
			// (address TRacker): tracker URL for BitTorrent downloads
			// Tracker URL; used to obtain resources for BitTorrent downloads without a need for DHT support.
			// The value must be URL encoded.
			// tr=http%3A%2F%2Fexample.com%2Fannounce
			r = manget_add_tr(magnet, query[i].value, query[i].n_value);
		}
		else if (query[i].n_name == 2 && 0 == strncmp("xl", query[i].name, 2))
		{
			// (eXact Length): size in bytes
			magnet->size = (uint64_t)strtoull(query[i].value, NULL, 10);
		}
		else if ( (query[i].n_name == 2 && 0 == strncmp("xs", query[i].name, 2)) || (query[i].n_name >= 3 && 0 == strncmp("xs.", query[i].name, 3)))
		{
			// (eXact Source): P2P link identified by a content-hash
			// either an HTTP (or HTTPS, FTP, FTPS, etc.) download source for the file pointed to by the 
			//Magnet link, the address of a P2P source for the file or the address of a hub (in the case 
			// of DC++), by which a client tries to connect directly, asking for the file and/or its sources. 
			// This field is commonly used by P2P clients to store the source, and may include the file hash.
			
			// 1. Content-Addressable Web URL
			//     This type of RFC 2168-based link is used by gnutella as well as G2 applications.[2]
			//     xs=http://[Client Address]:[Port of client]/uri-res/N2R?[ URN containing a file hash ]
			//     xs=http://192.0.2.27:6346/uri-res/N2R?urn:sha1:FINYVGHENTHSMNDSQQYDNLPONVBZTICF
			// 2. Link to a DirectConnect hub to find sources for a file
			//     This type of link connects a DirectConnect client immediately to the hub in question.
			//     xs=dchub://[hub address]:[hub port]
			// 3. Reference to a web-based source cache for a file on Gnutella2
			//     In this case, the included link points, not to a client IP or direct source, but to a source cache 
			//     which stores the IPs of other clients contacting it to download the same file. Once a client connects 
			//     to the cache, it is served IPs for alternate sources, while its own IP is stored within the cache and 
			//     forwarded to the next one connecting to the cache. This system operates similar to a BitTorrent tracker.
			//     xs=http://cache.freebase.be/[ SHA-1 hash ]
			// 4. Reference to an eD2k source
			//     xs=ed2kftp://[client address]:[client port]/[ed2k hash]/[file size]/
			r = manget_add_xs(magnet, query[i].value, query[i].n_value);
		}
		else if ( (query[i].n_name == 2 && 0 == strncmp("as", query[i].name, 2)) || (query[i].n_name >= 3 && 0 == strncmp("as.", query[i].name, 3)))
		{
			// (Acceptable Source): Web link to the file online
			// refers to a direct download from a web server. Regarded as only a fall-back source 
			// in case a client is unable to locate and/or download the linked-to file in its 
			// supported P2P network(s), most clients treat it equal to the "xs" token when it comes 
			// to priority, and ignore the timeout before contacting "as" sources denoted by the specs.
			// as=[ a web link to the file(URL encoded) ]
			r = manget_add_as(magnet, query[i].value, query[i].n_value);
		}
		else if (query[i].n_name == 2 && 0 == strncmp("dn", query[i].name, 2))
		{
			// (Display Name): a filename to display to the user, for convenience
			magnet->name = magnet_url_copy(magnet, query[i].value, query[i].n_value);
		}
		else if (query[i].n_name == 2 && 0 == strncmp("kt", query[i].name, 2))
		{
			// (Keyword Topic): a more general search, specifying keywords, rather than a particular file
			// This field specifies a string of search keywords to search for in P2P networks.
			// kt=kilroy+was+here+mp3
			magnet->keywords = magnet_url_copy(magnet, query[i].value, query[i].n_value);
		}
		else if (query[i].n_name == 2 && 0 == strncmp("mt", query[i].name, 2))
		{
			// (Manifest Topic): link to the metafile that contains a list of magneto (MAGMA ¨C MAGnet MAnifest)
			// This is a link to a list of links (see list). Perhaps as a web link...
			// mt=http://weblog.foo/all-my-favorites.rss
			// ...or a URN
			// mt=urn:sha1:3I42H3S6NNFQ2MSVX7XZKYAYSCX5QBYJ
			assert(0);
		}
		else if (query[i].n_name == 4 && 0 == strncmp("x.pe", query[i].name, 4))
		{
			assert(0);
		}
		else
		{
			// TODO:
			assert(0);
		}
	}

	uri_query_free(&query);

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
	//while (magnet->tracker_count > 0)
	//{
	//	free(magnet->trackers[--magnet->tracker_count]);
	//}
	free(magnet);
}

#if defined(_DEBUG) || defined(DEBUG)
void magnet_test(void)
{
	const uint8_t info_hash[] = { 0xde, 0xc8, 0xae, 0x69, 0x73, 0x51, 0xff, 0x4a, 0xec, 0x29, 0xcd, 0xba, 0xab, 0xf2, 0xfb, 0xe3, 0x46, 0x7c, 0xc2, 0x67 };
	const char* url = "magnet:?xt=urn:ed2k:354B15E68FB8F36D7CD88FF94116CDC1&xt=urn:tree:tiger:7N5OAMRNGMSSEUE3ORHOKWN4WWIQ5X4EBOOTLJY&xt=urn:btih:dec8ae697351ff4aec29cdbaabf2fbe3467cc267&xl=10826029&dn=mediawiki-1.15.1.tar.gz&tr=udp%3A%2F%2Ftracker.openbittorrent.com%3A80%2Fannounce&as=http%3A%2F%2Fdownload.wikimedia.org%2Fmediawiki%2F1.15%2Fmediawiki-1.15.1.tar.gz&xs=http%3A%2F%2Fcache.example.org%2FXRX2PEFXOOEJFRVUCX6HMZMKS5TWG4K5&xs=dchub://example.org";
	const uint8_t info_hash2[] = { 0xd2, 0x35, 0x40, 0x10, 0xa3, 0xca, 0x4a, 0xde, 0x5b, 0x74, 0x27, 0xbb, 0x09, 0x3a, 0x62, 0xa3, 0x89, 0x9f, 0xf3, 0x81 };
	const char* url2 = "magnet:?xt=urn:btih:2I2UAEFDZJFN4W3UE65QSOTCUOEZ744B&dn=Display%20Name&tr=http%3A%2F%2Ftracker.openbittorrent.com%2Fannounce&tr=http%3A%2F%2Ftracker.opentracker.org%2Fannounce";
	struct magnet_t* magnet;

	magnet = magnet_parse(url);
	assert( (MAGNET_HASH_BT|MAGNET_HASH_ED2K| MAGNET_HASH_TTH) == magnet->protocol);
	assert(0 == memcmp(magnet->bt_hash, info_hash, sizeof(magnet->bt_hash)));
	assert(10826029 == magnet->size);
	assert(1 == magnet->tracker_count);
	assert(0 == strcmp("mediawiki-1.15.1.tar.gz", magnet->name));
	assert(0 == strcmp(magnet->trackers[0], "udp://tracker.openbittorrent.com:80/announce"));
	assert(1 == magnet->as_count);
	assert(0 == strcmp(magnet->as[0], "http://download.wikimedia.org/mediawiki/1.15/mediawiki-1.15.1.tar.gz"));
	assert(2 == magnet->xs_count);
	assert(0 == strcmp(magnet->xs[0], "http://cache.example.org/XRX2PEFXOOEJFRVUCX6HMZMKS5TWG4K5"));
	assert(0 == strcmp(magnet->xs[1], "dchub://example.org"));
	magnet_free(magnet);

	magnet = magnet_parse(url2);
	assert(MAGNET_HASH_BT == magnet->protocol);
	assert(0 == memcmp(magnet->bt_hash, info_hash2, sizeof(magnet->bt_hash)));
	assert(0 == strcmp("Display Name", magnet->name));
	assert(2 == magnet->tracker_count);
	assert(0 == strcmp(magnet->trackers[0], "http://tracker.openbittorrent.com/announce"));
	assert(0 == strcmp(magnet->trackers[1], "http://tracker.opentracker.org/announce"));
	magnet_free(magnet);
}
#endif
