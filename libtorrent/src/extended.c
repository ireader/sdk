// http://www.bittorrent.org/beps/bep_0010.html (Extension Protocol)

#include "peer-extended.h"
#include "peer-message.h"
#include "byte-order.h"
#include "bencode.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

static int peer_extend_m_read(const struct bvalue_t* m, struct peer_extended_t* ext)
{
	char* p;
	size_t i;
	assert(BT_DICT == m->type);
	if (BT_DICT != m->type)
		return -1;
	
	for (i = 0; i < m->v.dict.count; i++)
	{
		if(m->v.dict.names[i].bytes < 3)
			continue;

		// To make sure the extension names do not collide by mistake, 
		// they should be prefixed with the two (or one) character code 
		// that is used to identify the client that introduced the extension. 
		// This applies for both the names of extension messages, 
		// and for any additional information put inside the top-level dictionary. 
		// All one and two byte identifiers are invalid to use unless defined by this specification.
		p = strchr(m->v.dict.names[i].name, '_');
		if (!p) continue;

		// BEP 10 => rationale
		// The convention is to use a two letter prefix on the extension message names, 
		// the prefix would identify the client first implementing the extension message. 
		// e.g. LT_metadata is implemented by libtorrent, and hence it has the LT prefix.
		if (0 == strcmp("pex", p + 1))
		{
			bencode_get_int(m->v.dict.values + i, &ext->m.pex);
		}
		else if (0 == strcmp("metadata", p + 1))
		{
			bencode_get_int(m->v.dict.values + i, &ext->m.metadata);
		}
		else if (0 == strcmp("holepunch", p + 1))
		{
			bencode_get_int(m->v.dict.values + i, &ext->m.holepunch);
		}
		else if (0 == strcmp("tex", p + 1))
		{
			bencode_get_int(m->v.dict.values + i, &ext->m.tex);
		}
	}

	return 0;
}

int peer_extended_read(const uint8_t* buffer, int bytes, struct peer_extended_t* ext)
{
	int r;
	size_t i, j;
	struct bvalue_t root;
	r = bencode_read(buffer, bytes, &root);
	if (r <= 0)
		return r;

	memset(ext, 0, sizeof(*ext));
	if (root.type == BT_DICT)
	{
		for (i = 0; i < root.v.dict.count; i++)
		{
			if (0 == strcmp("m", root.v.dict.names[i].name))
			{
				peer_extend_m_read(root.v.dict.values + i, ext);
			}
			else if (0 == strcmp("e", root.v.dict.names[i].name))
			{
				bencode_get_int(root.v.dict.values + i, &ext->encryption);
			}
			else if (0 == strcmp("p", root.v.dict.names[i].name))
			{
				bencode_get_int(root.v.dict.values + i, &ext->port);
			}
			else if (0 == strcmp("v", root.v.dict.names[i].name))
			{
				if (BT_STRING == root.v.dict.values[i].type) 
				{
					j = root.v.dict.values[i].v.str.bytes;
					j = j > (sizeof(ext->version) - 1) ? (sizeof(ext->version) - 1) : j;
					memcpy(ext->version, root.v.dict.values[i].v.str.value, j);
				}
			}
			else if (0 == strcmp("reqq", root.v.dict.names[i].name))
			{
				bencode_get_int(root.v.dict.values + i, &ext->reqq);
			}
			else if (0 == strcmp("yourip", root.v.dict.names[i].name)
				|| 0 == strcmp("ipv4", root.v.dict.names[i].name)
				|| 0 == strcmp("ipv6", root.v.dict.names[i].name))
			{
				if (BT_STRING == root.v.dict.values[i].type)
				{
					j = root.v.dict.values[i].v.str.bytes;
					assert(16 == j || 4 == j);
					j = j > sizeof(ext->ip) ? sizeof(ext->ip) : j;
					memcpy(ext->ip, root.v.dict.values[i].v.str.value, j);
					ext->ipv6 = 16 == j ? 1 : 0;
				}
			}
		}
	}

	bencode_free(&root);
	return 0;
}

int peer_extended_write(uint8_t* buffer, int bytes, uint16_t port, const char* version, int32_t metasize)
{
	int n;
	buffer[4] = BT_EXTENDED;
	buffer[5] = BT_EXTENDED_HANDSHAKE;

	// e : 0
	// m : ut_metadata : 2
	// m : ut_pex : 1
	// metadata_size : 31325
	// p : port
	// reqq : 255
	// v : version
	n = snprintf((char*)buffer + 6, bytes - 6, "d1:ei0e1:md11:ut_metadatai2e6:ut_pexi1ee13:metadata_sizei%de1:pi%hue4:reqqi255e1:v%u:%se", metasize, port, (unsigned int)strlen(version), version);
	if (n < 0 || n >= bytes - 6)
		return -1;

	nbo_w32(buffer, 2 + n);
	return 6 + n;
}
