// http://www.bittorrent.org/beps/bep_0009.html (Extension for Peers to Send Metadata Files)

#include "peer-extended.h"
#include "peer-message.h"
#include "byte-order.h"
#include "bencode.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

int peer_metadata_request_write(uint8_t* buffer, uint32_t bytes, uint32_t piece)
{
	int n;
	buffer[4] = BT_EXTENDED;
	buffer[5] = BT_EXTENDED_METADATA;

	// {'msg_type': 0, 'piece': 0}
	n = snprintf((char*)buffer + 6, bytes - 6, "d8:msg_typei0e5:piecei%uee", piece);
	nbo_w32(buffer, n + 2);
	return n;
}

int peer_metadata_reject_write(uint8_t* buffer, uint32_t bytes, uint32_t piece)
{
	int n;
	buffer[4] = BT_EXTENDED;
	buffer[5] = BT_EXTENDED_METADATA;

	// {'msg_type': 2, 'piece': 0}
	n = snprintf((char*)buffer + 6, bytes - 6, "d8:msg_typei2e5:piecei%uee", piece);
	nbo_w32(buffer, n + 2);
	return n;
}

int peer_metadata_data_write(uint8_t* buffer, uint32_t bytes, uint32_t piece, const uint8_t* data, uint32_t size)
{
	int n;
	buffer[4] = BT_EXTENDED;
	buffer[5] = BT_EXTENDED_METADATA;

	// If the piece is the last piece of the metadata, it may be less than 16kiB. 
	// If it is not the last piece of the metadata, it MUST be 16kiB.
	// {'msg_type': 1, 'piece': 0, 'total_size': 3425}
	n = snprintf((char*)buffer + 6, bytes - 6, "d8:msg_typei1e5:piecei%ue10:total_sizei%uee", piece, size);
	if (n < 0 || n + 6 + size > bytes)
		return -1;

	memcpy(buffer + n, data, size);
	nbo_w32(buffer, n + size + 2);
	return n + size + 6;
}

int peer_metadata_read(const uint8_t* buffer, uint32_t bytes, struct peer_metadata_handler_t* handler, void* param)
{
	int r;
	int32_t type, piece, size;
	struct bvalue_t root;
	const struct bvalue_t *tnode, *pnode, *nnode;
	r = bencode_read(buffer, bytes, &root);
	if (r <= 0)
		return r;

	tnode = bencode_find(&root, "msg_type");
	pnode = bencode_find(&root, "piece");
	if (tnode && pnode
		&& 0 == bencode_get_int(tnode, &type)
		&& 0 == bencode_get_int(pnode, &piece))
	{
		switch (type)
		{
		case 0:
			r = handler->request(param, piece);
			break;

		case 1:
			nnode = bencode_find(&root, "total_size");
			if (nnode && 0 == bencode_get_int(nnode, &size) && (uint32_t)size + r <= bytes)
				r = handler->data(param, piece, buffer, size);
			else
				r = -1;
			break;

		case 2:
			r = handler->reject(param, piece);
			break;

		default:
			r = 0;
			assert(0);
			break;
		}
	}

	bencode_free(&root);
	return r;
}
