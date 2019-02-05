// rfc3489 11.1 Message Header(p25)
// rfc5839 6. STUN Message Structure(p10)

#include "stun-message.h"
#include "stun-internal.h"
#include "byte-order.h"
#include "sha.h"
#include "md5.h"
#include "crc32.h"
#include <string.h>

int stun_header_read(const uint8_t* data, int bytes, struct stun_header_t* header)
{
	if (!data || bytes < 20)
		return -1;

	// The most significant 2 bits of every STUN message MUST be zeroes
	be_read_uint16(data, &header->msgtype);
	be_read_uint16(data + 2, &header->length);
	be_read_uint32(data + 4, &header->cookie);
	memcpy(header->tid, data + 8, 12);
	return 20;
}

int stun_header_write(const struct stun_header_t* header, uint8_t* data, int bytes)
{
	assert(0 == header->length % 4);
	if (!data || bytes < 20)
		return -1;

	// The most significant 2 bits of every STUN message MUST be zeroes
	data[0] = (uint8_t)((header->msgtype >> 8) & 0x3F); // STUN Message Type
	data[1] = (uint8_t)header->msgtype;
	data[2] = (uint8_t)(header->length >> 8); // Message Length
	data[3] = (uint8_t)header->length;

	be_write_uint32(data + 4, header->cookie); // Magic Cookie
	memcpy(data + 8, header->tid, 12); // Transaction ID (96 bits)
	return 20;
}

int stun_message_read(struct stun_message_t* msg, const uint8_t* data, int size)
{
	int r;
	r = stun_header_read(data, size, &msg->header);
	if (r < 0 || msg->header.length + r > size)
		return r;

	assert(20 == r && 0 == msg->header.length % 4);
	msg->nattrs = stun_attr_read(data + r, data + size, msg->attrs, sizeof(msg->attrs) / sizeof(msg->attrs[0]));
	if (msg->nattrs < 0)
		return msg->nattrs;
	return msg->nattrs < 0 ? msg->nattrs : 0;
}

int stun_message_write(uint8_t* data, int size, const struct stun_message_t* msg)
{
	int r;
	const uint8_t* p;
	r = stun_header_write(&msg->header, data, size);
	if (r < 0)
		return r;

	p = stun_attr_write(data + r, data + size, msg->attrs, msg->nattrs);
	return p >= data + size ? -1 : 0;
}

static void long_term_key(uint8_t md5[16], const char* username, const char* password, const char* realm)
{
	MD5_CTX ctx;
	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)username, strlen(username));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)realm, strlen(realm));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)password, strlen(password));
	MD5Final(md5, &ctx);
}

int stun_message_add_credentials(struct stun_message_t* msg, const char* username, const char* password, const char* realm, const char* nonce)
{
	int r, nkey;
	uint8_t md5[16];
	uint8_t sha1[20], *key;
	uint8_t data[1024];
	memset(sha1, 0, sizeof(sha1));

	if (username && *username && password && *password)
	{
		msg->attrs[msg->nattrs].type = STUN_ATTR_USERNAME;
		msg->attrs[msg->nattrs].length = (uint16_t)strlen(username);
		msg->attrs[msg->nattrs].v.data = username;
		msg->header.length += 8 + ALGIN_4BYTES(msg->attrs[msg->nattrs++].length);

		if (realm && *realm && nonce && *nonce)
		{
			msg->attrs[msg->nattrs].type = STUN_ATTR_REALM;
			msg->attrs[msg->nattrs].length = (uint16_t)strlen(realm);
			msg->attrs[msg->nattrs].v.data = realm;
			msg->header.length += 8 + ALGIN_4BYTES(msg->attrs[msg->nattrs++].length);

			msg->attrs[msg->nattrs].type = STUN_ATTR_NONCE;
			msg->attrs[msg->nattrs].length = (uint16_t)strlen(nonce);
			msg->attrs[msg->nattrs].v.data = nonce;
			msg->header.length += 8 + ALGIN_4BYTES(msg->attrs[msg->nattrs++].length);

			long_term_key(md5, username, password, realm);
			key = md5;
			nkey = sizeof(md5);
		}
		else
		{
			key = password;
			nkey = strlen(password);
		}

		// The length MUST then be set to point to the length of the message up to, and including,
		// the MESSAGE-INTEGRITY attribute itself, but excluding any attributes after it.
		msg->attrs[msg->nattrs].type = STUN_ATTR_MESSAGE_INTEGRITY;
		msg->attrs[msg->nattrs].length = sizeof(sha1);
		msg->attrs[msg->nattrs].v.data = sha1;
		msg->header.length += 8 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);

		r = stun_message_write(data, sizeof(data), msg);
		if (r < 0)
			return r;

		hmac(SHA1, data, msg->header.length + STUN_HEADER_SIZE, key, nkey, sha1);
		msg->attrs[msg->nattrs++].v.data = sha1;
	}

	return 0;
}

int stun_message_add_fingerprint(struct stun_message_t* msg)
{
	int r;
	uint8_t data[1024];

	// As with MESSAGE-INTEGRITY, the CRC used in the FINGERPRINT attribute
	// covers the length field from the STUN message header.
	msg->attrs[msg->nattrs].type = STUN_ATTR_FIGNERPRINT;
	msg->attrs[msg->nattrs].length = 4;
	msg->attrs[msg->nattrs].v.u32 = 0;
	msg->header.length += 8 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);

	r = stun_message_write(data, sizeof(data), msg);
	if (r < 0)
		return r;

	msg->attrs[msg->nattrs++].v.u32 = crc32(-1, data, msg->header.length + STUN_HEADER_SIZE);
	return 0;
}
