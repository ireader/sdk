#ifndef _peer_message_h_
#define _peer_message_h_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum peer_message_id_t
{
	BT_CHOKE = 0,
	BT_UNCHOKE,
	BT_INTERESTED,
	BT_NONINTERESTED,
	BT_HAVE,
	BT_BITFIELD,
	BT_REQUEST,
	BT_PIECE,
	BT_CANCEL,
	BT_PORT,
	BT_SUGGEST_PIECE = 13, // BEP6
	BT_HAVE_ALL = 14, // BEP6
	BT_HAVE_NONE = 15, // BEP6
	BT_REJECT = 16,  // BEP6
	BT_ALLOWD_FAST = 17, // BEP6
	BT_EXTENDED = 20,
	BT_HASH_REQUEST = 21,
	BT_HASHES,
	BT_HASH_REJECT,

	BT_HANDSHAKE = 254,
	BT_KEEPALIVE = 255,
};

int peer_handshake_read(const uint8_t* buffer, int bytes, uint8_t flags[8], uint8_t info_hash[20], uint8_t peer_id[20]);
int peer_handshake_write(uint8_t buffer[68], const uint8_t info_hash[20], const uint8_t peer_id[20]);

int peer_choke_read(const uint8_t* buffer, int bytes);
int peer_choke_write(uint8_t buffer[5]);

int peer_unchoke_read(const uint8_t* buffer, int bytes);
int peer_unchoke_write(uint8_t buffer[5]);

int peer_interested_read(const uint8_t* buffer, int bytes);
int peer_interested_write(uint8_t buffer[5]);

int peer_noninterested_read(const uint8_t* buffer, int bytes);
int peer_noninterested_write(uint8_t buffer[5]);

int peer_have_read(const uint8_t* buffer, int bytes, uint32_t* piece);
int peer_have_write(uint8_t buffer[9], uint32_t piece);

/// @param[in/out] count bitfield bytes(not bits)
int peer_bitfield_read(const uint8_t* buffer, int bytes, uint8_t** bitfield, uint32_t* count);
int peer_bitfield_write(uint8_t* buffer, int bytes, const uint8_t* bitfield, uint32_t count);

int peer_request_read(const uint8_t* buffer, int bytes, uint32_t* piece, uint32_t *begin, uint32_t *length);
int peer_request_write(uint8_t buffer[17], uint32_t piece, uint32_t begin, uint32_t length);

int peer_piece_read(const uint8_t* buffer, int bytes, uint32_t* piece, uint32_t *begin, uint32_t* length, uint8_t** data);
int peer_piece_write(uint8_t* buffer, int bytes, uint32_t piece, uint32_t begin, uint32_t length, const uint8_t* data);

int peer_cancel_read(const uint8_t* buffer, int bytes, uint32_t* piece, uint32_t *begin, uint32_t *length);
int peer_cancel_write(uint8_t buffer[17], uint32_t piece, uint32_t begin, uint32_t length);

int peer_port_read(const uint8_t* buffer, int bytes, uint16_t* port);
int peer_port_write(uint8_t buffer[7], uint16_t port);

#if defined(__cplusplus)
}
#endif
#endif /* !_peer_message_h_ */
