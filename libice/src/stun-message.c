// rfc3489 11.1 Message Header(p25)
// rfc5839 6. STUN Message Structure(p10)

#include "stun-message.h"
#include "stun-proto.h"
#include "byte-order.h"
#include "sha.h"
#include "md5.h"
#include "crc32.h"
#include <string.h>

void crc32_lsb_init(void);
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

const struct stun_attr_t* stun_message_attr_find(const struct stun_message_t* msg, uint16_t attr)
{
	int i;
	for (i = 0; i < msg->nattrs; i++)
	{
		if (msg->attrs[i].type == attr)
			return msg->attrs + i;
	}
	return NULL;
}

int stun_message_attr_list(const struct stun_message_t* msg, uint16_t attr, int(*fn)(void*, const struct stun_attr_t*), void* param)
{
	int i, r;
	for (r = i = 0; i < msg->nattrs && 0 == r; i++)
	{
		if (msg->attrs[i].type == attr)
			r = fn(param, msg->attrs + i);
	}
	return r;
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
	msg->attrs[msg->nattrs].v.ptr = (void*)value;
	msg->header.length += 4 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);
	msg->nattrs += 1;
	return 0;
}

int stun_message_add_address(struct stun_message_t* msg, uint16_t attr, const struct sockaddr* addr)
{
	msg->attrs[msg->nattrs].type = attr;
	msg->attrs[msg->nattrs].length = addr->sa_family == AF_INET6 ? 20 : 8;
	memcpy(&msg->attrs[msg->nattrs].v.addr, addr, socket_addr_len(addr));
	msg->header.length += 4 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);
	msg->nattrs += 1;
	return 0;
}

int stun_message_add_data(struct stun_message_t* msg, uint16_t attr, const void* value, int len)
{
	msg->attrs[msg->nattrs].type = attr;
	msg->attrs[msg->nattrs].length = (uint16_t)len;
	msg->attrs[msg->nattrs].v.ptr = (void*)value;
	msg->header.length += 4 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);
	msg->nattrs += 1;
	return 0;
}

int stun_message_add_error(struct stun_message_t* msg, uint32_t code, const char* phrase)
{
	assert(strlen(phrase) + 4 < 0xFF);
	msg->attrs[msg->nattrs].type = STUN_ATTR_ERROR_CODE;
	msg->attrs[msg->nattrs].length = (uint16_t)(4 + strlen(phrase));
	msg->attrs[msg->nattrs].v.errcode.code = code;
	msg->attrs[msg->nattrs].v.errcode.reason_phrase = (char*)phrase;
	msg->header.length += 4 + ALGIN_4BYTES(msg->attrs[msg->nattrs].length);
	msg->nattrs += 1;
	return 0;
}

static int stun_message_attr_detete(struct stun_message_t* msg, uint16_t attr)
{
    int i;
    for (i = 0; i < msg->nattrs; i++)
    {
        if (msg->attrs[i].type != attr)
            continue;
        
        msg->header.length -= 4 + ALGIN_4BYTES(msg->attrs[i].length);
        if(i + 1 < msg->nattrs)
            memmove(msg->attrs+i, msg->attrs + i + 1, (msg->nattrs - i - 1) * sizeof(msg->attrs[i]));
        msg->nattrs -= 1;
    }

    return -1; //  not found
}

static void long_term_key(uint8_t md5[16], const char* username, const char* password, const char* realm)
{
	MD5_CTX ctx;
	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)username, (unsigned int)strlen(username));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)realm, (unsigned int)strlen(realm));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)password, (unsigned int)strlen(password));
	MD5Final(md5, &ctx);
}

int stun_message_add_credentials(struct stun_message_t* msg, const struct stun_credential_t* auth)
{
	int r, nkey;
	uint8_t md5[16];
	const uint8_t *key;
	uint8_t data[1600];

	if (!*auth->usr|| !*auth->pwd)
		return -1; // invalid username/password

	if (STUN_CREDENTIAL_LONG_TERM == auth->credential && (!*auth->realm || !*auth->nonce))
		return -1; // invalid realm/nonce

    if(msg->nattrs > 1 && STUN_ATTR_FINGERPRINT == msg->attrs[msg->nattrs-1].type)
        stun_message_attr_detete(msg, STUN_ATTR_FINGERPRINT);
    if(msg->nattrs > 0 && STUN_ATTR_MESSAGE_INTEGRITY == msg->attrs[msg->nattrs-1].type)
    {
        stun_message_attr_detete(msg, STUN_ATTR_MESSAGE_INTEGRITY);
        stun_message_attr_detete(msg, STUN_ATTR_NONCE);
        stun_message_attr_detete(msg, STUN_ATTR_REALM);
        stun_message_attr_detete(msg, STUN_ATTR_USERNAME);
    }
    
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
		key = (const uint8_t *)auth->pwd;
		nkey = (int)strlen(auth->pwd);
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
    
    // rewrite fingerprint
    if(msg->nattrs > 0 && STUN_ATTR_FINGERPRINT == msg->attrs[msg->nattrs-1].type)
        stun_message_attr_detete(msg, STUN_ATTR_FINGERPRINT);
    
    // As with MESSAGE-INTEGRITY, the CRC used in the FINGERPRINT attribute
    // covers the length field from the STUN message header.
    msg->attrs[msg->nattrs].type = STUN_ATTR_FINGERPRINT;
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

int stun_message_check_integrity(const uint8_t* data, int bytes, const struct stun_message_t* msg, const struct stun_credential_t* auth)
{
	int nattrs;
	uint16_t len;
	uint8_t md5[16];
	uint8_t sha1[20];
	HMACContext context;

	if (msg->header.length + STUN_HEADER_SIZE != bytes || msg->header.length < 24)
		return -1;

	if (STUN_CREDENTIAL_LONG_TERM == auth->credential)
	{
		long_term_key(md5, auth->usr, auth->pwd, auth->realm);
		hmacReset(&context, SHA1, md5, sizeof(md5));
	}
	else
	{
		hmacReset(&context, SHA1, (const uint8_t *)auth->pwd, (int)strlen(auth->pwd));
	}

	len = msg->header.length;
	nattrs = msg->nattrs;
	if (nattrs > 0 && STUN_ATTR_FINGERPRINT == msg->attrs[nattrs - 1].type && msg->header.length > 8)
	{
		len -= 8;
		nattrs--;
	}
	if (nattrs < 1 || STUN_ATTR_MESSAGE_INTEGRITY != msg->attrs[nattrs - 1].type || msg->header.length < 24)
		return -1;

	md5[0] = (uint8_t)(len >> 8);
	md5[1] = (uint8_t)(len & 0xFF);
	hmacInput(&context, data, 2); // stun header message type
	hmacInput(&context, md5, 2); // stun header message length (filter fingerprint)
	hmacInput(&context, data + 4, 16 /*stun remain header*/ + len /* payload except fingerprint */ - (sizeof(sha1) + 4) /* sha1 */);
	hmacResult(&context, sha1);
	return memcmp(msg->attrs[nattrs - 1].v.sha1, sha1, sizeof(sha1));
}

int stun_message_check_fingerprint(const uint8_t* data, int bytes, const struct stun_message_t* msg)
{
	int nattrs;
	uint16_t len;
	uint32_t v;

	if (msg->header.length + STUN_HEADER_SIZE != bytes)
		return -1;

	nattrs = msg->nattrs;
	be_read_uint16(data + 2, &len);
	if (nattrs < 1 || STUN_ATTR_FINGERPRINT != msg->attrs[nattrs - 1].type)
		return -1;

	crc32_lsb_init();
	v = crc32_lsb(0xFFFFFFFF, data, msg->header.length + STUN_HEADER_SIZE - 8);
	v = v ^ STUN_FINGERPRINT_XOR;

	return msg->attrs[nattrs-1].v.u32 - v;
}

int stun_message_has_integrity(const struct stun_message_t* msg)
{
	int nattrs;
	nattrs = msg->nattrs;
	if (nattrs > 0 && STUN_ATTR_FINGERPRINT == msg->attrs[msg->nattrs - 1].type)
		nattrs--;
	return nattrs > 0 && STUN_ATTR_MESSAGE_INTEGRITY == msg->attrs[msg->nattrs - 1].type ? 1 : 0;
}

int stun_message_has_fingerprint(const struct stun_message_t* msg)
{
	return msg->nattrs > 0 && STUN_ATTR_FINGERPRINT == msg->attrs[msg->nattrs - 1].type ? 1 : 0;
}

#if defined(DEBUG) || defined(_DEBUG)
// https://tools.ietf.org/html/rfc5769#section-2.2
void stun_message_test(void)
{
	struct stun_credential_t auth;
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
	const uint8_t result2[] = {
		0x01, 0x01, 0x00, 0x3c, 0x21, 0x12, 0xa4, 0x42, 0xb7, 0xe7, 0xa7, 0x01, 0xbc, 0x34, 0xd6, 0x86, 
		0xfa, 0x87, 0xdf, 0xae, 0x80, 0x22, 0x00, 0x0b, 0x74, 0x65, 0x73, 0x74, 0x20, 0x76, 0x65, 0x63, 
		0x74, 0x6f, 0x72, 0x20, 0x00, 0x20, 0x00, 0x08, 0x00, 0x01, 0xa1, 0x47, 0xe1, 0x12, 0xa6, 0x43, 
		0x00, 0x08, 0x00, 0x14, 0x2b, 0x91, 0xf5, 0x99, 0xfd, 0x9e, 0x90, 0xc3, 0x8c, 0x74, 0x89, 0xf9, 
		0x2a, 0xf9, 0xba, 0x53, 0xf0, 0x6b, 0xe7, 0xd7, 0x80, 0x28, 0x00, 0x04, 0xc0, 0x7d, 0x4c, 0x96};
    
    int r;
    uint8_t data[2048];
    struct stun_message_t msg;
	struct stun_message_t resp;
    memset(&msg, 0, sizeof(msg));
    memset(data, 0x20, sizeof(data));
	memset(&auth, 0, sizeof(auth));
	snprintf(auth.usr, sizeof(auth.usr), "%s", "evtj:h6vY");
	snprintf(auth.pwd, sizeof(auth.pwd), "%s", "VOkJxbRl1RmTxUk/WvJxBt");

    msg.header.msgtype = STUN_METHOD_BIND;
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
	assert(0 == stun_message_check_integrity(data, msg.header.length + STUN_HEADER_SIZE, &msg, &auth));
	assert(0 == stun_message_check_fingerprint(data, msg.header.length + STUN_HEADER_SIZE, &msg));

	memset(&resp, 0, sizeof(resp));
	assert(0 == stun_message_read(&resp, result2, sizeof(result2)));
	assert(0 == memcmp(resp.header.tid, transaction, sizeof(transaction)));
	assert(resp.header.cookie == STUN_MAGIC_COOKIE);
	assert(0 == stun_message_check_integrity(result2, sizeof(result2), &resp, &auth));
	assert(0 == stun_message_check_fingerprint(result2, sizeof(result2), &resp));
}
#endif
