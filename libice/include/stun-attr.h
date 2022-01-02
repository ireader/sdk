#ifndef _stun_attr_h_
#define _stun_attr_h_

#include <stdint.h>
#include "sys/sock.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define ALGIN_4BYTES(v) (((v)+3) / 4 * 4)

struct stun_message_t;

struct stun_attr_t
{
	uint16_t type;
	uint16_t length;
	union
	{
		uint8_t					u8;
		uint16_t				u16;
		uint32_t				u32;
		uint64_t				u64;
		void*					ptr;
		uint8_t                 sha1[20];
		struct sockaddr_storage addr; // MAPPED-ADDRESS/XOR-MAPPED-ADDRESS
		struct {
			uint32_t			code;
			char*				reason_phrase;
		} errcode;
	} v;

	int unknown; // read(decode) only, -1-unknown attribute, other-valid
};

int stun_attr_read(const struct stun_message_t* msg, const uint8_t* data, const uint8_t* end, struct stun_attr_t *attrs, int n);

uint8_t* stun_attr_write(const struct stun_message_t* msg, uint8_t* data, const uint8_t* end, const struct stun_attr_t *attrs, int n);

#if defined(__cplusplus)
}
#endif
#endif /* !_stun_attr_h_ */
