#ifndef _stun_message_h_
#define _stun_message_h_

#include <stdint.h>
#include <assert.h>
#include "stun-attr.h"

#define STUN_ATTR_N 32

// rfc 3489 11.1 Message Header (p25)
struct stun_header_t
{
	uint16_t msgtype;
	uint16_t length; // payload length, don't include header
	uint32_t cookie; // rfc 5398 magic cookie
	uint8_t tid[12]; // transaction id
};

struct stun_message_t
{
	struct stun_header_t header;

	struct stun_attr_t attrs[STUN_ATTR_N];
	int nattrs;
};

/// @return <0-error, >0-stun header bytes
int stun_header_read(const uint8_t* data, int bytes, struct stun_header_t* header);
int stun_header_write(const struct stun_header_t* header, uint8_t* data, int bytes);

/// @return <0-error, >=0-stun message bytes
int stun_message_read(struct stun_message_t* msg, const uint8_t* data, int size);
int stun_message_write(uint8_t* data, int size, const struct stun_message_t* msg);

int stun_message_add_credentials(struct stun_message_t* msg, const char* username, const char* password, const char* realm, const char* nonce);
int stun_message_add_fingerprint(struct stun_message_t* msg);

int stun_transaction_id(uint8_t* id, int bytes);

#endif /* !_stun_message_h_ */
