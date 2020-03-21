#include "unicode.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif

int unicode_to_utf8(IN const wchar_t* src, IN size_t srcLen, OUT char* tgt, IN size_t tgtBytes)
{
#if (defined(_WIN32) || defined(_WIN64)) && 0
	srcLen = 0==srcLen?wcslen(src)+1:srcLen;
	return WideCharToMultiByte(CP_UTF8, 0, src, srcLen, tgt, tgtBytes, NULL, NULL);

#else
	size_t i;
	char* p = tgt;
	srcLen = 0==srcLen?wcslen(src)+1:srcLen;
	for(i=0; i<srcLen; i++)
	{
		if(src[i] <= 0x7F)
		{
			assert(p-tgt < (int)tgtBytes);
			// U+0000 to U+007F
			// binary: 0xxxxxxx
			// UTF-8:  0xxxxxxx
			*p++ = (char)src[i];
		}
		else if(src[i] <= 0x7FF)
		{
			assert(p+1-tgt < (int)tgtBytes);
			// U+0080 to U+07FF
			// binary: 00000yyy yyxxxxxx
			// UTF-8:  110yyyyy 10xxxxxx
			*p++ = (char)(0xc0 | ((src[i]>>6)&0x1F));
			*p++ = (char)(0x80 | (src[i]&0x3F));
		}
		else if(src[i] <= 0xFFFF)
		{
			assert(p+2-tgt < (int)tgtBytes);
			// U+0800 to U+FFFF
			// binary: zzzzyyyy yyxxxxxx
			// UTF-8:  1110zzzz 10yyyyyy 10xxxxxx
			*p++ = (char)(0xe0 | ((src[i]>>12)&0x0F));
			*p++ = (char)(0x80 | ((src[i]>>6)&0x3F));
			*p++ = (char)(0x80 | (src[i]&0x3F));
		}
		else if(src[i] <= 0x10FFFF)
		{
			assert(p+3-tgt < (int)tgtBytes);
			// U+010000 to U+10FFFF
			// binary: 000wwwzz zzzzyyyy yyxxxxxx
			// UTF-8:  11110www 10zzzzzz 10yyyyyy 10xxxxxx
			*p++ = (char)(0xF0 | (((int)src[i]>>18)&0x07));
			*p++ = (char)(0x80 | ((src[i]>>12)&0x3F));
			*p++ = (char)(0x80 | ((src[i]>>6)&0x3F));
			*p++ = (char)(0x80 | (src[i]&0x3F));
		}
	}
	return (int)(p-tgt);
#endif
}

int unicode_from_utf8(IN const char* src, IN size_t srcLen, OUT wchar_t* tgt, IN size_t tgtBytes)
{
#if (defined(_WIN32) || defined(_WIN64)) && 0
	srcLen = 0==srcLen?strlen(src)+1:srcLen;
	return MultiByteToWideChar(CP_UTF8, 0, src, srcLen, tgt, tgtBytes/sizeof(wchar_t));

#else
	size_t i;
	wchar_t* wc = tgt;
	srcLen = 0==srcLen?strlen(src)+1:srcLen;
	for(i=0; i<srcLen; i++)
	{
		if(0xF0 == (0xF0 & src[i]))
		{
			assert(i+3 < srcLen);
			assert(0x80 == (src[i+1]&0x80));
			assert(0x80 == (src[i+2]&0x80));
			assert(0x80 == (src[i+3]&0x80));

			// U+010000 to U+10FFFF
			// binary: 000wwwzz zzzzyyyy yyxxxxxx
			// UTF-8:  11110www 10zzzzzz 10yyyyyy 10xxxxxx
			*wc++ = ((((wchar_t)src[i])&0x07) << 18)
					| ((((wchar_t)src[i+1])&0x3F) << 12)
					| ((((wchar_t)src[i+2])&0x3F) << 6)
					| (((wchar_t)src[i+3])&0x3F);
			i += 3;
		}
		else if(0xE0 == (0xE0 & src[i]))
		{
			assert(i+2 < srcLen);
			assert(0x80 == (src[i+1]&0x80));
			assert(0x80 == (src[i+2]&0x80));

			// U+0800 to U+FFFF
			// binary: zzzzyyyy yyxxxxxx
			// UTF-8:  1110zzzz 10yyyyyy 10xxxxxx
			*wc++ = ((((wchar_t)src[i])&0x0F) << 12)
				| ((((wchar_t)src[i+1])&0x3F) << 6)
				| (((wchar_t)src[i+2])&0x3F);
			i += 2;
		}
		else if(0xC0 == (0xC0 & src[i]))
		{
			assert(i+1 < srcLen);
			assert(0x80 == (src[i+1]&0x80));
			
			// U+0080 to U+07FF
			// binary: 00000yyy yyxxxxxx
			// UTF-8:  110yyyyy 10xxxxxx
			*wc++ = ((((wchar_t)src[i])&0x1F) << 6)
				| (((wchar_t)src[i+1])&0x3F);
			i += 1;
		}
		else
		{
			// U+0000 to U+007F
			// binary: 0xxxxxxx
			// UTF-8:  0xxxxxxx
			*wc++ = ((wchar_t)src[i]) & 0xFF;
		}
	}
	return (int)(wc-tgt);
#endif
}

int unicode_to_mbcs(IN const wchar_t* src, IN size_t srcLen, OUT char* tgt, IN size_t tgtBytes)
{
	srcLen = 0==srcLen?wcslen(src)+1:srcLen;

#if defined(_WIN32) || defined(_WIN64)
	return WideCharToMultiByte(CP_ACP, 0, src, srcLen, tgt, tgtBytes, NULL, NULL);

#else
	if(tgtBytes > 0 && tgtBytes/sizeof(wchar_t) < srcLen)
	{
		assert(0);
		return -1;
	}
	return (int)wcstombs(tgt, src, tgtBytes);
#endif
}

int unicode_from_mbcs(IN const char* src, IN size_t srcLen, OUT wchar_t* tgt, IN size_t tgtBytes)
{
	srcLen = 0==srcLen?strlen(src)+1:srcLen;

#if defined(_WIN32) || defined(_WIN64)
	return MultiByteToWideChar(CP_ACP, 0, src, srcLen, tgt, tgtBytes/sizeof(wchar_t));

#else
	if(tgtBytes > 0 && tgtBytes/sizeof(wchar_t) < srcLen)
	{
		assert(0);
		return -1;
	}
	return (int)mbstowcs(tgt, src, tgtBytes);
#endif
}

extern int gb2312_mbtowc(const unsigned char *src, wchar_t *tgt, int tgtLen);
extern int gb2312_wctomb(const wchar_t* src, unsigned char *tgt, int tgtLen);

int unicode_to_gb18030(IN const wchar_t* src, IN size_t srcLen, OUT char* tgt, IN size_t tgtBytes)
{
	srcLen = 0==srcLen?wcslen(src)+1:srcLen;

#if defined(_WIN32) || defined(_WIN64)
	//936 gb2312
	return WideCharToMultiByte(54936, 0, src, srcLen, tgt, tgtBytes, NULL, NULL);

#else
	if(tgtBytes > 0 && tgtBytes/sizeof(wchar_t) < srcLen)
	{
		assert(0);
		return -1;
	}
	return gb2312_wctomb(src, (unsigned char*)tgt, (int)tgtBytes);
	//return wcstombs(tgt, src, tgtBytes);
#endif
}

int unicode_from_gb18030(IN const char* src, IN size_t srcLen, OUT wchar_t* tgt, IN size_t tgtBytes)
{
	srcLen = 0==srcLen?strlen(src)+1:srcLen;

#if defined(_WIN32) || defined(_WIN64)
	//936 gb2312
	return MultiByteToWideChar(54936, 0, src, srcLen, tgt, tgtBytes/sizeof(wchar_t));

#else
	if(tgtBytes > 0 && tgtBytes/sizeof(wchar_t) < srcLen)
	{
		assert(0);
		return -1;
	}
	return gb2312_mbtowc((unsigned char*)src, tgt, (int)tgtBytes);
	//return mbstowcs(tgt, src, tgtBytes);
#endif
}

#if defined(OS_LINUX)
#include "i18n/nls_cp936.c"
#endif
