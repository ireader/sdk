#include "base64.h"
#include <assert.h>
#include <stdint.h>

static char s_base64_enc[64] = {
	'A','B','C','D','E','F','G','H','I','J','K','L','M',
	'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
	'a','b','c','d','e','f','g','h','i','j','k','l','m',
	'n','o','p','q','r','s','t','u','v','w','x','y','z',
	'0','1','2','3','4','5','6','7','8','9','+','/'
};
static char s_base64_url[64] = {
	'A','B','C','D','E','F','G','H','I','J','K','L','M',
	'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
	'a','b','c','d','e','f','g','h','i','j','k','l','m',
	'n','o','p','q','r','s','t','u','v','w','x','y','z',
	'0','1','2','3','4','5','6','7','8','9','-','_'
};
static const uint8_t s_base64_dec[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,62, 0,62, 0,63,  /* +/-/ */
	52,53,54,55,56,57,58,59,60,61, 0, 0, 0, 0, 0, 0, /* 0 - 9 */
	0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* A - Z */
	15,16,17,18,19,20,21,22,23,24,25, 0, 0, 0, 0,63, /* _ */
	00,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40, /* a - z */
	41,42,43,44,45,46,47,48,49,50,51, 0, 0, 0, 0, 0,
};

static size_t base64_encode_table(char* target, const void *source, size_t bytes, const char* table)
{
	size_t i, j;
	const uint8_t *ptr = (const uint8_t*)source;

	for (j = i = 0; i < bytes / 3 * 3; i += 3)
	{
		target[j++] = table[(ptr[i] >> 2) & 0x3F]; /* c1 */
		target[j++] = table[((ptr[i] & 0x03) << 4) | ((ptr[i + 1] >> 4) & 0x0F)]; /*c2*/
		target[j++] = table[((ptr[i + 1] & 0x0F) << 2) | ((ptr[i + 2] >> 6) & 0x03)];/*c3*/
		target[j++] = table[ptr[i + 2] & 0x3F]; /* c4 */
	}

	if (i < bytes)
	{
		/* There were only 2 bytes in that last group */
		target[j++] = table[(ptr[i] >> 2) & 0x3F];

		if (i + 1 < bytes)
		{
			target[j++] = table[((ptr[i] & 0x03) << 4) | ((ptr[i + 1] >> 4) & 0x0F)]; /*c2*/
			target[j++] = table[((ptr[i + 1] & 0x0F) << 2)]; /*c3*/
		}
		else
		{
			/* There was only 1 byte in that last group */
			target[j++] = table[((ptr[i] & 0x03) << 4)]; /*c2*/
			target[j++] = '='; /*c3*/
		}

		target[j++] = '='; /*c4*/
	}

	return j;
}

size_t base64_encode(char* target, const void *source, size_t bytes)
{
	return base64_encode_table(target, source, bytes, s_base64_enc);
}

size_t base64_encode_url(char* target, const void *source, size_t bytes)
{
	return base64_encode_table(target, source, bytes, s_base64_url);
}

size_t base64_decode(void* target, const char *src, size_t bytes)
{
	size_t i;
	uint8_t* p = (uint8_t*)target;
	const uint8_t* source = (const uint8_t*)src;
	const uint8_t* end;

	//assert(0 == bytes % 4);

	end = source + bytes;
#define S(i) ((source+i < end) ? source[i] : '=')
	for (i = 0; source < end && '=' != *source; source += 4)
	{
		p[i++] = (s_base64_dec[S(0)] << 2) | (s_base64_dec[S(1)] >> 4);
		if ('=' != S(2)) p[i++] = (s_base64_dec[S(1)] << 4) | (s_base64_dec[S(2)] >> 2);
		if ('=' != S(3)) p[i++] = (s_base64_dec[S(2)] << 6) | s_base64_dec[S(3)];
	}
#undef S
	return i;
}

static const char* s_base16_enc = "0123456789ABCDEF";
static const uint8_t s_base16_dec[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, /* 0 - 9 */
	0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* A - F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* a - f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// RFC3548 6. Base 16 Encoding (p8)
size_t base16_encode(char* target, const void *source, size_t bytes)
{
	size_t i;
	const uint8_t* p;
	p = (const uint8_t*)source;
	for (i = 0; i < bytes; i++)
	{
		target[i * 2] = s_base16_enc[(*p >> 4) & 0x0F];
		target[i * 2 + 1] = s_base16_enc[*p & 0x0F];
		++p;
	}
	return bytes * 2;
}

size_t base16_decode(void* target, const char *source, size_t bytes)
{
	size_t i;
	uint8_t* p;
	p = (uint8_t*)target;
	assert(0 == bytes % 2);
	for (i = 0; i < bytes / 2; i++)
	{
		p[i] = s_base16_dec[(unsigned char)source[i * 2]] << 4;
		p[i] |= s_base16_dec[(unsigned char)source[i * 2 + 1]];
	}
	return i;
}

// https://en.wikipedia.org/wiki/Base32
static const char* s_base32_enc = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
static const uint8_t s_base32_dec[256] = {
	0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0, 0, 0, 0, 0, 0, 0, 0, /* 0 - 9 */
	0,    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, /* A - F */
	0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0, 0, 0, 0, 0,
	0,    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, /* a - f */
	0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0, 0, 0, 0, 0,
};

// RFC4648
size_t base32_encode(char* target, const void *source, size_t bytes)
{
	size_t i, j;
	const uint8_t *ptr = (const uint8_t*)source;

	for (j = i = 0; i < bytes / 5 * 5; i += 5)
	{
		target[j++] = s_base32_enc[(ptr[i] >> 3) & 0x1F]; /* c1 */
		target[j++] = s_base32_enc[((ptr[i] & 0x07) << 2) | ((ptr[i + 1] >> 6) & 0x03)]; /*c2*/
		target[j++] = s_base32_enc[(ptr[i + 1] >> 1) & 0x1F];/*c3*/
		target[j++] = s_base32_enc[((ptr[i + 1] & 0x01) << 4) | ((ptr[i + 2] >> 4) & 0x0F)]; /*c4*/
		target[j++] = s_base32_enc[((ptr[i + 2] & 0x0F) << 1) | ((ptr[i + 3] >> 7) & 0x01)]; /*c5*/
		target[j++] = s_base32_enc[(ptr[i + 3] >> 2) & 0x1F];/*c6*/
		target[j++] = s_base32_enc[((ptr[i + 3] & 0x03) << 3) | ((ptr[i + 4] >> 5) & 0x07)]; /*c7*/
		target[j++] = s_base32_enc[ptr[i + 4] & 0x1F]; /* c8 */
	}

	if (i + 1 == bytes)
	{
		target[j++] = s_base32_enc[(ptr[i] >> 3) & 0x1F]; /* c1 */
		target[j++] = s_base32_enc[((ptr[i] & 0x07) << 2)]; /*c2*/
	}
	else if (i + 2 == bytes)
	{
		target[j++] = s_base32_enc[(ptr[i] >> 3) & 0x1F]; /* c1 */
		target[j++] = s_base32_enc[((ptr[i] & 0x07) << 2) | ((ptr[i + 1] >> 6) & 0x03)]; /*c2*/
		target[j++] = s_base32_enc[(ptr[i + 1] >> 1) & 0x1F];/*c3*/
		target[j++] = s_base32_enc[((ptr[i + 1] & 0x01) << 4)]; /*c4*/
	}
	else if (i + 3 == bytes)
	{
		target[j++] = s_base32_enc[(ptr[i] >> 3) & 0x1F]; /* c1 */
		target[j++] = s_base32_enc[((ptr[i] & 0x07) << 2) | ((ptr[i + 1] >> 6) & 0x03)]; /*c2*/
		target[j++] = s_base32_enc[(ptr[i + 1] >> 1) & 0x1F];/*c3*/
		target[j++] = s_base32_enc[((ptr[i + 1] & 0x01) << 4) | ((ptr[i + 2] >> 4) & 0x0F)]; /*c4*/
		target[j++] = s_base32_enc[((ptr[i + 2] & 0x0F) << 1)]; /*c5*/
	}
	else if (i + 4 == bytes)
	{
		target[j++] = s_base32_enc[(ptr[i] >> 3) & 0x1F]; /* c1 */
		target[j++] = s_base32_enc[((ptr[i] & 0x07) << 2) | ((ptr[i + 1] >> 6) & 0x03)]; /*c2*/
		target[j++] = s_base32_enc[(ptr[i + 1] >> 1) & 0x1F];/*c3*/
		target[j++] = s_base32_enc[((ptr[i + 1] & 0x01) << 4) | ((ptr[i + 2] >> 4) & 0x0F)]; /*c4*/
		target[j++] = s_base32_enc[((ptr[i + 2] & 0x0F) << 1) | ((ptr[i + 3] >> 7) & 0x01)]; /*c5*/
		target[j++] = s_base32_enc[(ptr[i + 3] >> 2) & 0x1F];/*c6*/
		target[j++] = s_base32_enc[((ptr[i + 3] & 0x03) << 3)]; /*c7*/
	}

	while (0 != (j % 8))
	{
		target[j++] = '=';
	}

	return j;
}

size_t base32_decode(void* target, const char *src, size_t bytes)
{
	size_t i;
	uint8_t* p = (uint8_t*)target;
	const uint8_t* source = (const uint8_t*)src;
	const uint8_t* end;

	end = source + bytes;
#define S(i) ((source+i < end) ? source[i] : '=')
	for(i = 0; source < end && '=' != *source; source += 8)
	{
		p[i++] = (s_base32_dec[S(0)] << 3) | (s_base32_dec[S(1)] >> 2);
		if ('=' != S(2)) p[i++] = (s_base32_dec[S(1)] << 6) | (s_base32_dec[S(2)] << 1) | (s_base32_dec[S(3)] >> 4);
		if ('=' != S(4)) p[i++] = (s_base32_dec[S(3)] << 4) | (s_base32_dec[S(4)] >> 1);
		if ('=' != S(5)) p[i++] = (s_base32_dec[S(4)] << 7) | (s_base32_dec[S(5)] << 2) | (s_base32_dec[S(6)] >> 3);
		if ('=' != S(7)) p[i++] = (s_base32_dec[S(6)] << 5) | s_base32_dec[S(7)];
	}
#undef S

	return i;
}

#if defined(DEBUG) || defined(_DEBUG)
#include <string.h>
void base64_test(void)
{
	const char* s;
	const char* r;
	char source[512];
	char target[512];
	const uint8_t p[] = { 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x21, 0xde, 0xad, 0xbe, 0xef };

	assert(8 == base64_encode(source, "4444", 4) && 0 == memcmp(source, "NDQ0NA==", 8));
	assert(4 == base64_decode(target, "NDQ0NA======", 12) && 0 == memcmp(target, "4444", 4));
	assert(4 == base64_decode(target, "NDQ0NA==", 8) && 0 == memcmp(target, "4444", 4));
	assert(4 == base64_decode(target, "NDQ0NA=", 7) && 0 == memcmp(target, "4444", 4));
	assert(4 == base64_decode(target, "NDQ0NA", 6) && 0 == memcmp(target, "4444", 4));

	assert(8 == base64_encode(source, "55555", 5) && 0 == memcmp(source, "NTU1NTU=", 8));
	assert(5 == base64_decode(target, "NTU1NTU=", 8) && 0 == memcmp(target, "55555", 5));
	assert(5 == base64_decode(target, "NTU1NTU", 7) && 0 == memcmp(target, "55555", 5));

	assert(8 == base64_encode(source, "666666", 6) && 0 == memcmp(source, "NjY2NjY2", 8));
	assert(6 == base64_decode(target, "NjY2NjY2====", 12) && 0 == memcmp(target, "666666", 6));
	assert(6 == base64_decode(target, "NjY2NjY2", 8) && 0 == memcmp(target, "666666", 6));

	// RFC2617 2.Basic Authentication Scheme (p6)
	s = "Aladdin:open sesame";
	r = "QWxhZGRpbjpvcGVuIHNlc2FtZQ==";
	assert(strlen(r) == base64_encode(source, s, strlen(s)) && 0 == memcmp(source, r, strlen(r)));
	assert(strlen(s) == base64_decode(target, source, strlen(r)) && 0 == memcmp(target, s, strlen(s)));

	assert(8 == base16_encode(source, "4444", 4));
	assert(4 == base16_decode(target, source, 8) && 0 == memcmp(target, "4444", 4));

	assert(16 == base32_encode(source, p, 10) && 0 == memcmp(source, "JBSWY3DPEHPK3PXP", 16));
	assert(10 == base32_decode(target, source, 16) && 0 == memcmp(target, p, sizeof(p)));
	assert(8 == base32_encode(source, "H", 1) && 0 == memcmp(source, "JA======", 8));
	assert(1 == base32_decode(target, source, 8) && 0 == memcmp(target, "H", 1));
	assert(1 == base32_decode(target, "JA", 2) && 0 == memcmp(target, "H", 1));
	assert(8 == base32_encode(source, "He", 2) && 0 == memcmp(source, "JBSQ====", 8));
	assert(2 == base32_decode(target, source, 8) && 0 == memcmp(target, "He", 2));
	assert(2 == base32_decode(target, "JBSQ", 4) && 0 == memcmp(target, "He", 2));
	assert(8 == base32_encode(source, "Hel", 3) && 0 == memcmp(source, "JBSWY===", 8));
	assert(3 == base32_decode(target, source, 8) && 0 == memcmp(target, "Hel", 3));
	assert(3 == base32_decode(target, "JBSWY", 5) && 0 == memcmp(target, "Hel", 3));
	assert(8 == base32_encode(source, "Hell", 4) && 0 == memcmp(source, "JBSWY3A=", 8));
	assert(4 == base32_decode(target, source, 8) && 0 == memcmp(target, "Hell", 4));
	assert(4 == base32_decode(target, "JBSWY3A", 7) && 0 == memcmp(target, "Hell", 4));
}
#endif
