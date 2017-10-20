// http://www.bittorrent.org/beps/bep_0003.html (The BitTorrent Protocol Specification)
// http://www.bittorrent.org/beps/bep_0052.html (The BitTorrent Protocol Specification v2)
// http://www.bittorrent.org/beps/bep_0029.html (uTorrent transport protocol)
// http://www.bittorrent.org/beps/bep_0005.html (DHT Protocol)
// http://www.bittorrent.org/beps/bep_0010.html (Extension Protocol)

#include "peer-message.h"
#include "byte-order.h"
#include "bencode.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

static const char* s_bittorrent_protocol = "BitTorrent protocol";

int peer_handshake_read(const uint8_t* buffer, int bytes, uint8_t flags[8], uint8_t info_hash[20], uint8_t peer_id[20])
{
	if (bytes < 68 || 19 != buffer[0] || 0 != memcmp(s_bittorrent_protocol, buffer + 1, 19))
		return -1;

	memcpy(flags, buffer + 20, 8);
	memcpy(info_hash, buffer + 28, 20);
	memcpy(peer_id, buffer + 48, 20);
	return 68;
}

int peer_handshake_write(uint8_t buffer[68], const uint8_t info_hash[20], const uint8_t peer_id[20])
{
	buffer[0] = 19;
	memcpy(buffer + 1, s_bittorrent_protocol, 19);
	memset(buffer + 20, 0, 8);
	memcpy(buffer + 28, info_hash, 20);
	memcpy(buffer + 48, peer_id, 20);
	buffer[25] = 0x10; // extended
	return 68;
}

int peer_choke_read(const uint8_t* buffer, int bytes)
{
	(void)buffer, (void)bytes;
	return 0;
}

int peer_choke_write(uint8_t buffer[5])
{
	nbo_w32(buffer, 1);
	buffer[4] = BT_CHOKE;
	return 5;
}

int peer_unchoke_read(const uint8_t* buffer, int bytes)
{
	(void)buffer, (void)bytes;
	return 0;
}

int peer_unchoke_write(uint8_t buffer[5])
{
	nbo_w32(buffer, 1);
	buffer[4] = BT_UNCHOKE;
	return 5;
}

int peer_interested_read(const uint8_t* buffer, int bytes)
{
	(void)buffer, (void)bytes;
	return 0;
}

int peer_interested_write(uint8_t buffer[5])
{
	nbo_w32(buffer, 1);
	buffer[4] = BT_INTERESTED;
	return 5;
}

int peer_noninterested_read(const uint8_t* buffer, int bytes)
{
	(void)buffer, (void)bytes;
	return 0;
}

int peer_noninterested_write(uint8_t buffer[5])
{
	nbo_w32(buffer, 1);
	buffer[4] = BT_NONINTERESTED;
	return 5;
}

int peer_have_read(const uint8_t* buffer, int bytes, uint32_t* piece)
{
	if (bytes < 4)
		return -1;
	nbo_r32(buffer, piece);
	return 4;
}

int peer_have_write(uint8_t buffer[9], uint32_t piece)
{
	nbo_w32(buffer, 5);
	buffer[4] = BT_PIECE;
	nbo_w32(buffer + 5, piece);
	return 9;
}

int peer_bitfield_read(const uint8_t* buffer, int bytes, uint8_t** bitfield, uint32_t* count)
{
	if (bytes < 4)
		return -1;

	*bitfield = malloc(bytes);
	if (!*bitfield)
		return -1;
	*count = bytes;
	memcpy(*bitfield, buffer, bytes);
	return bytes;
}

int peer_bitfield_write(uint8_t* buffer, int bytes, const uint8_t* bitfield, uint32_t count)
{
	if (bytes < count + 5)
		return -1;
	nbo_w32(buffer, 1 + count);
	buffer[4] = BT_BITFIELD;
	memcpy(buffer + 5, bitfield, count);
	return 5 + count;
}

int peer_request_read(const uint8_t* buffer, int bytes, uint32_t* piece, uint32_t *begin, uint32_t *length)
{
	if (bytes < 12)
		return -1;
	nbo_r32(buffer, piece);
	nbo_r32(buffer + 4, begin);
	nbo_r32(buffer + 8, length);
	return 12;
}

int peer_request_write(uint8_t buffer[17], uint32_t piece, uint32_t begin, uint32_t length)
{
	nbo_w32(buffer, 13);
	buffer[4] = BT_REQUEST;
	nbo_w32(buffer + 5, piece);
	nbo_w32(buffer + 9, begin);
	nbo_w32(buffer + 13, length);
	return 17;
}

int peer_piece_read(const uint8_t* buffer, int bytes, uint32_t* piece, uint32_t *begin, uint32_t* length, uint8_t** data)
{
	if (bytes < 8)
		return -1;

	nbo_r32(buffer, piece);
	nbo_r32(buffer + 4, begin);

	*data = malloc(bytes - 8);
	if (!*data)
		return -1;

	*length = bytes - 8;
	memcpy(*data, buffer + 8, bytes - 8);
	return bytes;
}

int peer_piece_write(uint8_t* buffer, int bytes, uint32_t piece, uint32_t begin, uint32_t length, const uint8_t* data)
{
	if (bytes < 13 + length)
		return -1;
	nbo_w32(buffer, 9 + length);
	buffer[4] = BT_PIECE;
	nbo_w32(buffer + 5, piece);
	nbo_w32(buffer + 9, begin);
	memcpy(buffer + 13, data, length);
	return 13 + length;
}

int peer_cancel_read(const uint8_t* buffer, int bytes, uint32_t* piece, uint32_t *begin, uint32_t *length)
{
	if (bytes < 12)
		return -1;
	nbo_r32(buffer, piece);
	nbo_r32(buffer + 4, begin);
	nbo_r32(buffer + 8, length);
	return 12;
}

int peer_cancel_write(uint8_t buffer[17], uint32_t piece, uint32_t begin, uint32_t length)
{
	nbo_w32(buffer, 13);
	buffer[4] = BT_CANCEL;
	nbo_w32(buffer + 5, piece);
	nbo_w32(buffer + 9, begin);
	nbo_w32(buffer + 13, length);
	return 17;
}

int peer_port_read(const uint8_t* buffer, int bytes, uint16_t* port)
{
	if (bytes < 2)
		return -1;
	nbo_r16(buffer, port);
	return 2;
}

int peer_port_write(uint8_t buffer[7], uint16_t port)
{
	nbo_w32(buffer, 3);
	buffer[4] = BT_PORT;
	nbo_w16(buffer + 5, port);
	return 7;
}

int peer_extended_read(const uint8_t* buffer, int bytes)
{
	int r;
	struct bvalue_t root;
	assert(bytes >= 1);
	if (0 == buffer[0])
	{
		r = bencode_read(buffer + 1, bytes - 1, &root);
		if (0 != r)
			return r;

		if (root.type == BT_DICT)
		{
		}

		bencode_free(&root);
	}
	return 0;
}

int peer_extended_write(uint8_t* buffer, int bytes, uint16_t port, const char* version)
{
	int n;
	buffer[4] = BT_EXTENDED;
	buffer[5] = 0; // extended message ID. 0 = handshake

	// e : 0
	// m : ut_metadata : 2
	// m : ut_pex : 1
	// p : port
	// reqq : 255
	// v : version
	n = snprintf((char*)buffer + 6, bytes - 6, "d1:ei0e1:md11:ut_metadatai2e6:ut_pexi1ee1:pi%hue4:reqqi255e1:v%u:%se", port, (unsigned int)strlen(version), version);
	if (n < 0 || n >= bytes - 6)
		return -1;

	nbo_w32(buffer, 2 + n);
	return 6 + n;
}
