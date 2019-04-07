#include "stun-attr.h"
#include "stun-proto.h"
#include "stun-message.h"
#include "byte-order.h"
#include <string.h>
#include <assert.h>

// 14.6. EVEN-PORT (rfc5766 p46)
static inline const uint8_t* stun_attr_read_uint8(const uint8_t* p, const uint8_t* end, uint8_t* v)
{
	if (p + ALGIN_4BYTES(1) > end)
		return end;

	*v = *p;
	return p + ALGIN_4BYTES(1);
}

static inline uint8_t* stun_attr_write_uint8(uint8_t* p, const uint8_t* end, const uint8_t v)
{
	if (p + ALGIN_4BYTES(1) > end)
		return (uint8_t*)end;

	*p = v;
	return p + ALGIN_4BYTES(1);
}

// 14.6. EVEN-PORT (rfc5766 p46)
static inline const uint8_t* stun_attr_read_uint16(const uint8_t* p, const uint8_t* end, uint16_t* v)
{
	if (p + ALGIN_4BYTES(2) > end)
		return end;

	be_read_uint16(p, v);
	return p + ALGIN_4BYTES(2);
}

static inline uint8_t* stun_attr_write_uint16(uint8_t* p, const uint8_t* end, const uint16_t v)
{
	if (p + ALGIN_4BYTES(2) > end)
		return (uint8_t*)end;

	be_write_uint16(p, v);
	return p + ALGIN_4BYTES(2);
}

// 14.1. CHANNEL-NUMBER (rfc5766 p45)
// 14.2. LIFETIME (rfc5766 p46)
static inline const uint8_t* stun_attr_read_uint32(const uint8_t* p, const uint8_t* end, uint32_t* v)
{
	if (p + ALGIN_4BYTES(4) > end)
		return end;

	be_read_uint32(p, v);
	return p + ALGIN_4BYTES(4);
}

static inline uint8_t* stun_attr_write_uint32(uint8_t* p, const uint8_t* end, uint32_t v)
{
	if (p + ALGIN_4BYTES(4) > end)
		return (uint8_t*)end;

	be_write_uint32(p, v);
	return p + ALGIN_4BYTES(4);
}

// 14.4. DATA (rfc5766 p46)
static inline const uint8_t* stun_attr_read_bytes(const struct stun_message_t* msg, const uint8_t* p, const uint8_t* end, int length, uint8_t* data, int size)
{
	assert(size >= length);
	if (p + ALGIN_4BYTES(length) > end)
		return end;

	memmove(data, p, size < length ? size : length);
	return p + ALGIN_4BYTES(length);
}

static inline uint8_t* stun_attr_write_bytes(const struct stun_message_t* msg, uint8_t* p, const uint8_t* end, const uint8_t* data, int size)
{
	if (p + ALGIN_4BYTES(size) > end)
		return (uint8_t*)end;

	memmove(p, data, size);
	return p + ALGIN_4BYTES(size);
}

static int stun_attr_error_code_read(const struct stun_message_t* msg, const uint8_t* data, struct stun_attr_t *attr)
{
	be_read_uint32(data, &attr->v.errcode.code);
	attr->v.errcode.reason_phrase = data + 4;
	return 0;
}

static int stun_attr_error_code_write(const struct stun_message_t* msg, uint8_t* data, const struct stun_attr_t *attr)
{
	assert(attr->length >= 4);
	be_write_uint32(data, attr->v.errcode.code);
	memmove(data + 4, attr->v.errcode.reason_phrase, attr->length - 4);
    if(0 != attr->length % 4)
        memset(data + attr->length, 0x20, 4 - (attr->length % 4)); // string fill with 0x20 ' '
	return 0;
}

static int stun_attr_flag_read(const struct stun_message_t* msg, const uint8_t* data, struct stun_attr_t *attr)
{
	attr->v.u8 = 1;
	return 0;
}

static int stun_attr_flag_write(const struct stun_message_t* msg, uint8_t* data, const struct stun_attr_t *attr)
{
	return 0;
}

static int stun_attr_uint8_read(const struct stun_message_t* msg, const uint8_t* data, struct stun_attr_t *attr)
{
	attr->v.u8 = *data;
	return 0;
}

static int stun_attr_uint8_write(const struct stun_message_t* msg, uint8_t* data, const struct stun_attr_t *attr)
{
    *(uint32_t*)data = 0;
	*data = attr->v.u8;
	return 0;
}

static int stun_attr_uint32_read(const struct stun_message_t* msg, const uint8_t* data, struct stun_attr_t *attr)
{
	be_read_uint32(data, &attr->v.u32);
	return 0;
}

static int stun_attr_uint32_write(const struct stun_message_t* msg, uint8_t* data, const struct stun_attr_t *attr)
{
	be_write_uint32(data, attr->v.u32);
	return 0;
}

static int stun_attr_uint64_read(const struct stun_message_t* msg, const uint8_t* data, struct stun_attr_t *attr)
{
	be_read_uint64(data, &attr->v.u64);
	return 0;
}

static int stun_attr_uint64_write(const struct stun_message_t* msg, uint8_t* data, const struct stun_attr_t *attr)
{
	be_write_uint64(data, attr->v.u64);
	return 0;
}

static int stun_attr_ptr_read(const struct stun_message_t* msg, const uint8_t* data, struct stun_attr_t *attr)
{
	attr->v.ptr = data;
	return 0;
}

static int stun_attr_ptr_write(const struct stun_message_t* msg, uint8_t* data, const struct stun_attr_t *attr)
{
	memmove(data, attr->v.ptr, attr->length);
    if(0 != attr->length % 4)
        memset(data + attr->length, 0x20, 4 - (attr->length % 4)); // string fill with 0x20 ' '
	return 0;
}

static int stun_attr_sha1_read(const struct stun_message_t* msg, const uint8_t* data, struct stun_attr_t *attr)
{
    memmove(attr->v.sha1, data, sizeof(attr->v.sha1));
    return 0;
}

static int stun_attr_sha1_write(const struct stun_message_t* msg, uint8_t* data, const struct stun_attr_t *attr)
{
    assert(0 == sizeof(attr->v.sha1) % 4);
    memmove(data, attr->v.sha1, sizeof(attr->v.sha1));
    return 0;
}

static int stun_attr_mapped_address_read(const struct stun_message_t* msg, const uint8_t* data, struct stun_attr_t *attr)
{
	struct sockaddr_in* addr4;
	struct sockaddr_in6* addr6;

	memset(&attr->v.addr, 0, sizeof(attr->v.addr));
	if (1 == data[1])
	{
		attr->v.addr.ss_family = AF_INET;
		addr4 = (struct sockaddr_in*)&attr->v.addr;
		be_read_uint16(data + 2, &addr4->sin_port);
		be_read_uint32(data + 4, &addr4->sin_addr.s_addr);
	}
	else if (2 == data[1])
	{
		attr->v.addr.ss_family = AF_INET6;
		addr6 = (struct sockaddr_in6*)&attr->v.addr;
		be_read_uint16(data + 2, &addr6->sin6_port);
		memmove(addr6->sin6_addr.s6_addr, data + 4, sizeof(addr6->sin6_addr.s6_addr));
	}
	else
	{
		return -1;
	}

	return 0;
}

static int stun_attr_mapped_address_write(const struct stun_message_t* msg, uint8_t* data, const struct stun_attr_t *attr)
{
	const struct sockaddr_in* addr4;
	const struct sockaddr_in6* addr6;

	data[0] = 0;
	if (AF_INET == attr->v.addr.ss_family)
	{
		data[1] = 1;
		addr4 = (const struct sockaddr_in*)&attr->v.addr;
		be_write_uint16(data + 2, (uint16_t)addr4->sin_port);
        be_write_uint32(data + 4, addr4->sin_addr.s_addr);
	}
	else if (AF_INET6 == attr->v.addr.ss_family)
	{
		data[1] = 2;
		addr6 = (const struct sockaddr_in6*)&attr->v.addr;
		be_write_uint16(data + 2, (uint16_t)addr6->sin6_port);
		memmove(data + 4, addr6->sin6_addr.s6_addr, sizeof(addr6->sin6_addr.s6_addr));
	}
	else
	{
		return -1;
	}

	return 0;
}

static int stun_attr_xor_mapped_address_read(const struct stun_message_t* msg, const uint8_t* data, struct stun_attr_t *attr)
{
    int i;
    struct sockaddr_in* addr4;
    struct sockaddr_in6* addr6;
    
    memset(&attr->v.addr, 0, sizeof(attr->v.addr));
    if (1 == data[1])
    {
        attr->v.addr.ss_family = AF_INET;
        addr4 = (struct sockaddr_in*)&attr->v.addr;
        be_read_uint16(data + 2, &addr4->sin_port);
        be_read_uint32(data + 4, &addr4->sin_addr.s_addr);
        addr4->sin_port ^= (uint16_t)(msg->header.cookie >> 16);
        addr4->sin_addr.s_addr ^= msg->header.cookie;
    }
    else if (2 == data[1])
    {
        attr->v.addr.ss_family = AF_INET6;
        addr6 = (struct sockaddr_in6*)&attr->v.addr;
        be_read_uint16(data + 2, &addr6->sin6_port);
        addr6->sin6_addr.s6_addr[0] = data[4] ^ (uint8_t)(msg->header.cookie >> 24);
        addr6->sin6_addr.s6_addr[1] = data[5] ^ (uint8_t)(msg->header.cookie >> 16);
        addr6->sin6_addr.s6_addr[2] = data[6] ^ (uint8_t)(msg->header.cookie >> 8);
        addr6->sin6_addr.s6_addr[3] = data[7] ^ (uint8_t)(msg->header.cookie >> 0);
        for(i = 0; i < 12; i++)
            addr6->sin6_addr.s6_addr[4+i] = data[8+i] ^ msg->header.tid[i];
    }
    else
    {
        return -1;
    }
    
    return 0;
}

static int stun_attr_xor_mapped_address_write(const struct stun_message_t* msg, uint8_t* data, const struct stun_attr_t *attr)
{
    int i;
    const struct sockaddr_in* addr4;
    const struct sockaddr_in6* addr6;
    
    data[0] = 0;
    if (AF_INET == attr->v.addr.ss_family)
    {
        data[1] = 1;
        addr4 = (const struct sockaddr_in*)&attr->v.addr;
        be_write_uint16(data + 2, (((uint16_t)addr4->sin_port) ^ (uint16_t)(msg->header.cookie >> 16)));
        be_write_uint32(data + 4, addr4->sin_addr.s_addr ^ msg->header.cookie);
    }
    else if (AF_INET6 == attr->v.addr.ss_family)
    {
        data[1] = 2;
        addr6 = (const struct sockaddr_in6*)&attr->v.addr;
        be_write_uint16(data + 2, (((uint16_t)addr6->sin6_port) ^ (uint16_t)(msg->header.cookie >> 16)));
        data[4] = addr6->sin6_addr.s6_addr[0] ^ (uint8_t)(msg->header.cookie >> 24);
        data[5] = addr6->sin6_addr.s6_addr[1] ^ (uint8_t)(msg->header.cookie >> 16);
        data[6] = addr6->sin6_addr.s6_addr[2] ^ (uint8_t)(msg->header.cookie >> 8);
        data[7] = addr6->sin6_addr.s6_addr[3] ^ (uint8_t)(msg->header.cookie >> 0);
        for(i = 0; i < 12; i++)
            data[8 + i] = addr6->sin6_addr.s6_addr[4+i] ^ msg->header.tid[i];
    }
    else
    {
        return -1;
    }
    
    return 0;
}

static struct
{
	uint16_t type;
	int (*read)(const struct stun_message_t* msg, const uint8_t* data, struct stun_attr_t *attr);
	int (*write)(const struct stun_message_t* msg, uint8_t* data, const struct stun_attr_t *attr);
} s_stun_attrs[] = {
	{ STUN_ATTR_MAPPED_ADDRESS,		stun_attr_mapped_address_read,	    stun_attr_mapped_address_write },
	{ STUN_ATTR_RESPONSE_ADDRESS,	stun_attr_mapped_address_read,	    stun_attr_mapped_address_write },
	{ STUN_ATTR_CHANGE_REQUEST,		stun_attr_mapped_address_read,	    stun_attr_mapped_address_write },
	{ STUN_ATTR_SOURCE_ADDRESS,		stun_attr_mapped_address_read,	    stun_attr_mapped_address_write },
	{ STUN_ATTR_CHANGED_ADDRESS,	stun_attr_uint32_read,			    stun_attr_uint32_write },
	{ STUN_ATTR_USERNAME,			stun_attr_ptr_read,					stun_attr_ptr_write },
	{ STUN_ATTR_PASSWORD,			stun_attr_ptr_read,					stun_attr_ptr_write },
	{ STUN_ATTR_MESSAGE_INTEGRITY,	stun_attr_sha1_read,			    stun_attr_sha1_write },
	{ STUN_ATTR_ERROR_CODE,			stun_attr_error_code_read,		    stun_attr_error_code_write },
	{ STUN_ATTR_UNKNOWN_ATTRIBUTES, stun_attr_ptr_read,					stun_attr_ptr_write },
	{ STUN_ATTR_REFLECTED_FROM,		stun_attr_mapped_address_read,	    stun_attr_mapped_address_write },
	{ STUN_ATTR_CHANNEL_NUMBER,		stun_attr_uint32_read,			    stun_attr_uint32_write },
	{ STUN_ATTR_LIFETIME,			stun_attr_uint32_read,			    stun_attr_uint32_write },
	{ STUN_ATTR_BANDWIDTH,			stun_attr_uint32_read,			    stun_attr_uint32_write },
	{ STUN_ATTR_XOR_PEER_ADDRESS,	stun_attr_xor_mapped_address_read,	stun_attr_xor_mapped_address_write },
	{ STUN_ATTR_DATA,				stun_attr_ptr_read,					stun_attr_ptr_write },
	{ STUN_ATTR_REALM,				stun_attr_ptr_read,					stun_attr_ptr_write },
	{ STUN_ATTR_NONCE,				stun_attr_ptr_read,					stun_attr_ptr_write },
	{ STUN_ATTR_XOR_RELAYED_ADDRESS,stun_attr_xor_mapped_address_read,	stun_attr_xor_mapped_address_write },
	{ STUN_ATTR_EVEN_PORT,			stun_attr_uint8_read,			    stun_attr_uint8_write },
	{ STUN_ATTR_REQUESTED_TRANSPORT,stun_attr_uint32_read,			    stun_attr_uint32_write },
	{ STUN_ATTR_DONT_FRAGMENT,		stun_attr_flag_read,			    stun_attr_flag_write }, // flag(no value)
	{ STUN_ATTR_XOR_MAPPED_ADDRESS, stun_attr_xor_mapped_address_read,	stun_attr_xor_mapped_address_write },
	{ STUN_ATTR_TIMER_VAL,			stun_attr_ptr_read,					stun_attr_ptr_write },
	{ STUN_ATTR_RESERVATION_TOKEN,	stun_attr_uint64_read,			    stun_attr_uint64_write },
	{ STUN_ATTR_SOFTWARE,			stun_attr_ptr_read,					stun_attr_ptr_write },
	{ STUN_ATTR_ALTERNATE_SERVER,	stun_attr_mapped_address_read,	    stun_attr_mapped_address_write },
	{ STUN_ATTR_FIGNERPRINT,		stun_attr_uint32_read,			    stun_attr_uint32_write },
    { STUN_ATTR_PRIORITY,           stun_attr_uint32_read,              stun_attr_uint32_write },
    { STUN_ATTR_USE_CANDIDATE,      stun_attr_flag_read,                stun_attr_flag_write }, // flag(no value)
    { STUN_ATTR_ICE_CONTROLLED,     stun_attr_uint64_read,              stun_attr_uint64_write },
    { STUN_ATTR_ICE_CONTROLLING,    stun_attr_uint64_read,              stun_attr_uint64_write },
};

static inline int stun_attr_find(uint16_t type)
{
	int j;
	for (j = 0; j < sizeof(s_stun_attrs) / sizeof(s_stun_attrs[0]); j++)
	{
		if (s_stun_attrs[j].type == type)
			return j;
	}
	return -1;
}

int stun_attr_read(const struct stun_message_t* msg, const uint8_t* data, const uint8_t* end, struct stun_attr_t *attrs, int n)
{
	int i, j, r;
	for (r = i = 0; data + 4 <= end && i < n; i++)
	{
		be_read_uint16(data, &attrs[i].type);
		be_read_uint16(data + 2, &attrs[i].length);

		if (data + 4 + attrs[i].length > end)
			return -1;

		j = stun_attr_find(attrs[i].type);
		if (-1 != j)
			r = s_stun_attrs[j].read(msg, data + 4, attrs + i);
		else
			attrs[i].v.ptr = (void*)(data + 4);

		assert(r <= 0);
		if (r < 0)
			return r;

		data += 4 + ALGIN_4BYTES(attrs[i].length);
	}

	return i;
}

uint8_t* stun_attr_write(const struct stun_message_t* msg, uint8_t* data, const uint8_t* end, const struct stun_attr_t *attrs, int n)
{
	int i, j, r;

	for (r = i = 0; i < n; i++)
	{
		if (data + 4 + ALGIN_4BYTES(attrs[i].length) > end)
			return (uint8_t*)end; // not enough memory

		be_write_uint16(data, attrs[i].type);
		be_write_uint16(data + 2, attrs[i].length);

		j = stun_attr_find(attrs[i].type);
		if (-1 != j)
			r = s_stun_attrs[j].write(msg, data + 4, attrs + i);
		else
			memmove(data + 4, attrs[i].v.data, attrs[i].length);

		assert(r <= 0);
		if (r < 0)
			return (uint8_t*)end;

		data += 4 + ALGIN_4BYTES(attrs[i].length);
	}

	return data;
}
