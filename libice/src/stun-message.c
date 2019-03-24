// rfc3489 11.1 Message Header(p25)
// rfc5839 6. STUN Message Structure(p10)

#include "stun-message.h"
#include "byte-order.h"
#include "sha.h"
#include "md5.h"
#include "crc32.h"
#include <string.h>

void crc32_lsb_init();
unsigned int crc32_lsb(unsigned int crc, const unsigned char *buffer, unsigned int size);

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
	msg->nattrs = stun_attr_read(msg, data + r, data + size, msg->attrs, sizeof(msg->attrs) / sizeof(msg->attrs[0]));
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

	p = stun_attr_write(msg, data + r, data + size, msg->attrs, msg->nattrs);
	return p >= data + size ? -1 : 0;
}

int stun_message_attr_find(struct stun_message_t* msg, uint16_t attr)
{
	int i;
	for (i = 0; i < msg->nattrs; i++)
	{
		if (msg->attrs[i].type == attr)
			return i;
	}
	return -1;
}

int stun_message_add_flag(struct stun_message_t* msg, uint16_t attr)
{
	msg->attrs[msg->nattrs].type = attr;
	msg->attrs[msg->nattrs].length = 0;
	msg->header.length += 4;
	msg->nattrs += 1;
	return 0;
}

int stun_message_add_uint8(struct stun_message_t* msg, uint16_t attr, uint8_t value)
{
	msg->attrs[msg->nattrs].type = attr;
	msg->attrs[msg->nattrs].length = 1;
	msg->attrs[msg->nattrs].v.u64 = value;
	msg->header.length += 4 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);
	msg->nattrs += 1;
	return 0;
}

int stun_message_add_uint16(struct stun_message_t* msg, uint16_t attr, uint16_t value)
{
	msg->attrs[msg->nattrs].type = attr;
	msg->attrs[msg->nattrs].length = 2;
	msg->attrs[msg->nattrs].v.u64 = value;
	msg->header.length += 4 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);
	msg->nattrs += 1;
	return 0;
}

int stun_message_add_uint32(struct stun_message_t* msg, uint16_t attr, uint32_t value)
{
	msg->attrs[msg->nattrs].type = attr;
	msg->attrs[msg->nattrs].length = 4;
	msg->attrs[msg->nattrs].v.u64 = value;
	msg->header.length += 4 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);
	msg->nattrs += 1;
	return 0;
}

int stun_message_add_uint64(struct stun_message_t* msg, uint16_t attr, uint64_t value)
{
	msg->attrs[msg->nattrs].type = attr;
	msg->attrs[msg->nattrs].length = 8;
	msg->attrs[msg->nattrs].v.u64 = value;
	msg->header.length += 4 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);
	msg->nattrs += 1;
	return 0;
}

int stun_message_add_string(struct stun_message_t* msg, uint16_t attr, const char* value)
{
	msg->attrs[msg->nattrs].type = attr;
	msg->attrs[msg->nattrs].length = (uint16_t)strlen(value);
	msg->attrs[msg->nattrs].v.string = value;
	msg->header.length += 4 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);
	msg->nattrs += 1;
	return 0;
}

int stun_message_add_address(struct stun_message_t* msg, uint16_t attr, const struct sockaddr_storage* addr)
{
	msg->attrs[msg->nattrs].type = attr;
	msg->attrs[msg->nattrs].length = addr->ss_family == AF_INET6 ? 17 : 5;
	memcpy(&msg->attrs[msg->nattrs].v.addr, addr, sizeof(*addr));
	msg->header.length += 4 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);
	msg->nattrs += 1;
	return 0;
}

int stun_message_add_error(struct stun_message_t* msg, uint32_t code, const char* phrase)
{
	msg->attrs[msg->nattrs].type = STUN_ATTR_ERROR_CODE;
	msg->attrs[msg->nattrs].length = 4 + strlen(phrase);
	msg->attrs[msg->nattrs].v.errcode.code = code;
	msg->attrs[msg->nattrs].v.errcode.reason_phrase = phrase;
	msg->header.length += 4 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);
	msg->nattrs += 1;
	return 0;
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

int stun_message_add_credentials(struct stun_message_t* msg, const struct stun_credetial_t* auth)
{
	int r, nkey;
	uint8_t md5[16];
	uint8_t *key;
	uint8_t data[1600];

	if (!*auth->usr|| !*auth->pwd)
		return -1; // invalid username/password

	if (STUN_CREDENTIAL_LONG_TERM == auth->credential && (!*auth->realm || !*auth->nonce))
		return -1; // invalid realm/nonce

	stun_message_add_string(msg, STUN_ATTR_USERNAME, auth->usr);

	if (STUN_CREDENTIAL_LONG_TERM == auth->credential)
	{
		stun_message_add_string(msg, STUN_ATTR_REALM, auth->realm);
		stun_message_add_string(msg, STUN_ATTR_NONCE, auth->nonce);

		long_term_key(md5, auth->usr, auth->pwd, auth->realm);
		key = md5;
		nkey = sizeof(md5);
	}
	else
	{
		key = auth->pwd;
		nkey = strlen(auth->pwd);
	}

	// The length MUST then be set to point to the length of the message up to, and including,
	// the MESSAGE-INTEGRITY attribute itself, but excluding any attributes after it.
	msg->attrs[msg->nattrs].type = STUN_ATTR_MESSAGE_INTEGRITY;
	msg->attrs[msg->nattrs].length = sizeof(msg->attrs[msg->nattrs].v.sha1);
	msg->header.length += 4 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);

	r = stun_message_write(data, sizeof(data), msg);
	if (r < 0)
		return r;

	hmac(SHA1, data, msg->header.length + STUN_HEADER_SIZE - sizeof(msg->attrs[msg->nattrs].v.sha1) - 4, key, nkey, msg->attrs[msg->nattrs].v.sha1);
    msg->nattrs += 1;

	return 0;
}

int stun_message_add_fingerprint(struct stun_message_t* msg)
{
	int r;
    uint32_t v;
	uint8_t data[1600];
    
	// As with MESSAGE-INTEGRITY, the CRC used in the FINGERPRINT attribute
	// covers the length field from the STUN message header.
	msg->attrs[msg->nattrs].type = STUN_ATTR_FIGNERPRINT;
	msg->attrs[msg->nattrs].length = 4;
	msg->attrs[msg->nattrs].v.u32 = 0;
	msg->header.length += 4 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);

	r = stun_message_write(data, sizeof(data), msg);
	if (r < 0)
		return r;

    crc32_lsb_init();
    v = crc32_lsb(0xFFFFFFFF, data, msg->header.length + STUN_HEADER_SIZE - 8);
    msg->attrs[msg->nattrs].v.u32 = v ^ STUN_FINGERPRINT_XOR;
    msg->nattrs += 1;
	return 0;
}

#if defined(DEBUG) || defined(_DEBUG)
// https://tools.ietf.org/html/rfc5769#section-2.2
void stun_message_test(void)
{
	struct stun_credetial_t auth;
	const char* software = "STUN test client";
	const uint8_t transaction[] = { 0xb7, 0xe7, 0xa7, 0x01, 0xbc, 0x34, 0xd6, 0x86, 0xfa, 0x87, 0xdf, 0xae };
    const uint8_t result[] = {
        0x00, 0x01, 0x00, 0x58, 0x21, 0x12, 0xa4, 0x42, 0xb7, 0xe7, 0xa7, 0x01, 0xbc, 0x34, 0xd6, 0x86,
        0xfa, 0x87, 0xdf, 0xae, 0x80, 0x22, 0x00, 0x10, 0x53, 0x54, 0x55, 0x4e, 0x20, 0x74, 0x65, 0x73,
        0x74, 0x20, 0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x00, 0x24, 0x00, 0x04, 0x6e, 0x00, 0x01, 0xff,
        0x80, 0x29, 0x00, 0x08, 0x93, 0x2f, 0xf9, 0xb1, 0x51, 0x26, 0x3b, 0x36, 0x00, 0x06, 0x00, 0x09,
        0x65, 0x76, 0x74, 0x6a, 0x3a, 0x68, 0x36, 0x76, 0x59, 0x20, 0x20, 0x20, 0x00, 0x08, 0x00, 0x14,
        0x9a, 0xea, 0xa7, 0x0c, 0xbf, 0xd8, 0xcb, 0x56, 0x78, 0x1e, 0xf2, 0xb5, 0xb2, 0xd3, 0xf2, 0x49,
        0xc1, 0xb5, 0x71, 0xa2, 0x80, 0x28, 0x00, 0x04, 0xe5, 0x7a, 0x3b, 0xcf };
    
    int r;
    uint8_t data[2048];
    struct stun_message_t msg;
    memset(&msg, 0, sizeof(msg));
    memset(data, 0x20, sizeof(data));
	memset(&auth, 0, sizeof(auth));
	snprintf(auth.usr, sizeof(auth.usr), "%s", "evtj:h6vY");
	snprintf(auth.pwd, sizeof(auth.pwd), "%s", "VOkJxbRl1RmTxUk/WvJxBt");

    msg.header.msgtype = STUN_MESSAGE_TYPE(STUN_METHOD_CLASS_REQUEST, STUN_METHOD_BIND);
    msg.header.length = 0;
    msg.header.cookie = STUN_MAGIC_COOKIE;
    memcpy(msg.header.tid, transaction, sizeof(msg.header.tid));
    
	stun_message_add_string(&msg, STUN_ATTR_SOFTWARE, software);
	stun_message_add_uint32(&msg, STUN_ATTR_PRIORITY, 0x6e0001ff);
	stun_message_add_uint64(&msg, STUN_ATTR_ICE_CONTROLLED, 0x932ff9b151263b36);
    
    r = stun_message_add_credentials(&msg, &auth);
    r = 0 == r ? stun_message_add_fingerprint(&msg) : r;
    r = 0 == r ? stun_message_write(data, sizeof(data), &msg) : r;
    assert(0 == r);
    assert(sizeof(result) == msg.header.length + STUN_HEADER_SIZE && 0 == memcmp(data, result, sizeof(result)));
}
#endif
