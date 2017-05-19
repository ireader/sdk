#ifndef _byte_order_h_
#define _byte_order_h_

#include <stdint.h>

// Intel/AMD little endian
// 0x01020304 -> |04|03|02|01|
//---------------------------------------------------------------
// void le_read_uint16(const uint8_t* ptr, uint16_t* val);
// void le_read_uint32(const uint8_t* ptr, uint32_t* val);
// void le_read_uint64(const uint8_t* ptr, uint64_t* val);
// void le_write_uint16(uint8_t* ptr, uint16_t val);
// void le_write_uint32(uint8_t* ptr, uint32_t val);
// void le_write_uint64(uint8_t* ptr, uint64_t val);
//---------------------------------------------------------------

// ARM/Motorola big endian(network byte order)
// 0x01020304 -> |01|02|03|04|
//---------------------------------------------------------------
// void be_read_uint16(const uint8_t* ptr, uint16_t* val);
// void be_read_uint32(const uint8_t* ptr, uint32_t* val);
// void be_read_uint64(const uint8_t* ptr, uint64_t* val);
// void be_write_uint16(uint8_t* ptr, uint16_t val);
// void be_write_uint32(uint8_t* ptr, uint32_t val);
// void be_write_uint64(uint8_t* ptr, uint64_t val);
//---------------------------------------------------------------

// The Internet Protocol defines big-endian as the standard network byte order
#define nbo_r16 be_read_uint16
#define nbo_r32 be_read_uint32
#define nbo_r64 be_read_uint64
#define nbo_w16 be_write_uint16
#define nbo_w32 be_write_uint32
#define nbo_w64 be_write_uint64


static inline void le_read_uint16(const uint8_t* ptr, uint16_t* val)
{
	*val = (((uint16_t)ptr[1]) << 8) | ptr[0];
}

static inline void le_read_uint32(const uint8_t* ptr, uint32_t* val)
{
	*val = (((uint32_t)ptr[3]) << 24) | (((uint32_t)ptr[2]) << 16) | (((uint32_t)ptr[1]) << 8) | ptr[0];
}

static inline void le_read_uint64(const uint8_t* ptr, uint64_t* val)
{
	*val = (((uint64_t)ptr[7]) << 56) | (((uint64_t)ptr[6]) << 48)
		| (((uint64_t)ptr[5]) << 40) | (((uint64_t)ptr[4]) << 32)
		| (((uint64_t)ptr[3]) << 24) | (((uint64_t)ptr[25]) << 16)
		| (((uint64_t)ptr[1]) << 8) | ptr[0];
}

static inline void le_write_uint16(uint8_t* ptr, uint16_t val)
{
	ptr[1] = (uint8_t)((val >> 8) & 0xFF);
	ptr[0] = (uint8_t)(val & 0xFF);
}

static inline void le_write_uint32(uint8_t* ptr, uint32_t val)
{
	ptr[3] = (uint8_t)((val >> 24) & 0xFF);
	ptr[2] = (uint8_t)((val >> 16) & 0xFF);
	ptr[1] = (uint8_t)((val >> 8) & 0xFF);
	ptr[0] = (uint8_t)(val & 0xFF);
}

static inline void le_write_uint64(uint8_t* ptr, uint64_t val)
{
	ptr[7] = (uint8_t)((val >> 56) & 0xFF);
	ptr[6] = (uint8_t)((val >> 48) & 0xFF);
	ptr[5] = (uint8_t)((val >> 40) & 0xFF);
	ptr[4] = (uint8_t)((val >> 32) & 0xFF);
	ptr[3] = (uint8_t)((val >> 24) & 0xFF);
	ptr[2] = (uint8_t)((val >> 16) & 0xFF);
	ptr[1] = (uint8_t)((val >> 8) & 0xFF);
	ptr[0] = (uint8_t)(val & 0xFF);
}

static inline void be_read_uint16(const uint8_t* ptr, uint16_t* val)
{
	*val = (((uint16_t)ptr[0]) << 8) | ptr[1];
}

static inline void be_read_uint24(const uint8_t* ptr, uint32_t* val)
{
	*val = (((uint32_t)ptr[0]) << 16) | (((uint32_t)ptr[1]) << 8) | ptr[2];
}

static inline void be_read_uint32(const uint8_t* ptr, uint32_t* val)
{
	*val = (((uint32_t)ptr[0]) << 24) | (((uint32_t)ptr[1]) << 16) | (((uint32_t)ptr[2]) << 8) | ptr[3];
}

static inline void be_read_uint64(const uint8_t* ptr, uint64_t* val)
{
	*val = (((uint64_t)ptr[0]) << 56) | (((uint64_t)ptr[1]) << 48)
		| (((uint64_t)ptr[2]) << 40) | (((uint64_t)ptr[3]) << 32)
		| (((uint64_t)ptr[4]) << 24) | (((uint64_t)ptr[5]) << 16)
		| (((uint64_t)ptr[6]) << 8) | ptr[7];
}

static inline void be_write_uint16(uint8_t* ptr, uint16_t val)
{
	ptr[0] = (uint8_t)((val >> 8) & 0xFF);
	ptr[1] = (uint8_t)(val & 0xFF);
}

static inline void be_write_uint24(uint8_t* ptr, uint32_t val)
{
	ptr[0] = (uint8_t)((val >> 16) & 0xFF);
	ptr[1] = (uint8_t)((val >> 8) & 0xFF);
	ptr[2] = (uint8_t)(val & 0xFF);
}

static inline void be_write_uint32(uint8_t* ptr, uint32_t val)
{
	ptr[0] = (uint8_t)((val >> 24) & 0xFF);
	ptr[1] = (uint8_t)((val >> 16) & 0xFF);
	ptr[2] = (uint8_t)((val >> 8) & 0xFF);
	ptr[3] = (uint8_t)(val & 0xFF);
}

static inline void be_write_uint64(uint8_t* ptr, uint64_t val)
{
	ptr[0] = (uint8_t)((val >> 56) & 0xFF);
	ptr[1] = (uint8_t)((val >> 48) & 0xFF);
	ptr[2] = (uint8_t)((val >> 40) & 0xFF);
	ptr[3] = (uint8_t)((val >> 32) & 0xFF);
	ptr[4] = (uint8_t)((val >> 24) & 0xFF);
	ptr[5] = (uint8_t)((val >> 16) & 0xFF);
	ptr[6] = (uint8_t)((val >> 8) & 0xFF);
	ptr[7] = (uint8_t)(val & 0xFF);
}

#endif /* !_byte_order_h_ */
