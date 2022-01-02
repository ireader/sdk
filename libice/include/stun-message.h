#ifndef _stun_message_h_
#define _stun_message_h_

#include <stdint.h>
#include <assert.h>
#include "stun-attr.h"
#include "stun-proto.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define STUN_ATTR_N 32

// rfc5389 19. Changes since RFC 3489 (46)
// REALM, SERVER, reason phrases, and NONCE limited to 127 characters. USERNAME to 513 bytes
struct stun_credential_t
{
	int credential; // STUN_CREDENTIAL_SHORT_TERM/STUN_CREDENTIAL_LONG_TERM
	char usr[STUN_LIMIT_USERNAME_MAX];
	char pwd[STUN_LIMIT_USERNAME_MAX];
	char realm[128], nonce[128];
};

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

int stun_message_add_flag(struct stun_message_t* msg, uint16_t attr);
int stun_message_add_uint8(struct stun_message_t* msg, uint16_t attr, uint8_t value);
int stun_message_add_uint16(struct stun_message_t* msg, uint16_t attr, uint16_t value);
int stun_message_add_uint32(struct stun_message_t* msg, uint16_t attr, uint32_t value);
int stun_message_add_uint64(struct stun_message_t* msg, uint16_t attr, uint64_t value);
int stun_message_add_string(struct stun_message_t* msg, uint16_t attr, const char* value);
int stun_message_add_address(struct stun_message_t* msg, uint16_t attr, const struct sockaddr* addr);
int stun_message_add_data(struct stun_message_t* msg, uint16_t attr, const void* value, int len);

int stun_message_add_error(struct stun_message_t* msg, uint32_t code, const char* phrase);
int stun_message_add_credentials(struct stun_message_t* msg, const struct stun_credential_t* auth);
int stun_message_add_fingerprint(struct stun_message_t* msg);

int stun_message_check_integrity(const uint8_t* data, int bytes, const struct stun_message_t* msg, const struct stun_credential_t* auth);
int stun_message_check_fingerprint(const uint8_t* data, int bytes, const struct stun_message_t* msg);
int stun_message_has_integrity(const struct stun_message_t* msg);
int stun_message_has_fingerprint(const struct stun_message_t* msg);

const struct stun_attr_t* stun_message_attr_find(const struct stun_message_t* msg, uint16_t attr);
int stun_message_attr_list(const struct stun_message_t* msg, uint16_t attr, int (*fn)(void*, const struct stun_attr_t*), void* param);

int stun_transaction_id(uint8_t* id, int bytes);

int stun_credential_setauth(struct stun_credential_t* auth, int credential, const char* usr, const char* pwd, const char* realm, const char* nonce);

#if defined(__cplusplus)
}
#endif
#endif /* !_stun_message_h_ */
