#ifndef _StrConvert_h_
#define _StrConvert_h_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

class ToAString
{
public:
	ToAString(int v)
	{
		sprintf(_buffer, "%i", v);
	}

	ToAString(unsigned int v)
	{
		sprintf(_buffer, "%u", v);
	}

	ToAString(bool v)
	{
		if(v)
			strcpy(_buffer, "true");
		else
			strcpy(_buffer, "false");
	}

	ToAString(float v)
	{
		sprintf(_buffer, "%f", v);
	}

	ToAString(double v)
	{
		sprintf(_buffer, "%f", v);
	}

	ToAString(long long v)
	{
		sprintf(_buffer, "%lli", v);
	}

	operator const char*()
	{
		return _buffer;
	}

private:
	char _buffer[64];
};

class ToWString
{
public:
	ToWString(int v)
	{
		swprintf(_buffer, sizeof(_buffer)/sizeof(_buffer[0]), L"%i", v);
	}

	ToWString(unsigned int v)
	{
		swprintf(_buffer, sizeof(_buffer)/sizeof(_buffer[0]), L"%u", v);
	}

	ToWString(bool v)
	{
		if(v)
			wcscpy(_buffer, L"true");
		else
			wcscpy(_buffer, L"false");
	}

	ToWString(float v)
	{
		swprintf(_buffer, sizeof(_buffer)/sizeof(_buffer[0]), L"%f", v);
	}

	ToWString(double v)
	{
		swprintf(_buffer, sizeof(_buffer)/sizeof(_buffer[0]), L"%f", v);
	}

	ToWString(long long v)
	{
		swprintf(_buffer, sizeof(_buffer)/sizeof(_buffer[0]), L"%lli", v);
	}

	operator const wchar_t*()
	{
		return _buffer;
	}

private:
	wchar_t _buffer[64];
};

#if defined(UNICODE) || defined(_UNICODE)
	typedef ToWString ToString;
#else
	typedef ToAString ToString;
#endif


inline int str2int_radix2(const char* p)
{
	int v = 0;
	for(; p; ++p)
	{
		if('0'!=*p && '1'!=*p)
			break;

		v = v*2 + (*p - '0');
	}
	return v;
}

inline int str2int_radix8(const char* p)
{
	int v = 0;
	for(; p; ++p)
	{
		if('0'>*p || '7'<*p)
			break;

		v = v*8 + (*p - '0');
	}
	return v;
}

inline int str2int_radix16(const char* p)
{
	int v = 0;
	for(; p; ++p)
	{
		if('0'<=*p && '9'>=*p)
		{
			v = v*16 + (*p - '0');
		}
		else if('a'<=*p && 'f'>=*p)
		{
			v = v*16 + (*p - 'a' + 10);
		}
		else if('A'<=*p && 'F'>=*p)
		{
			v = v*16 + (*p - 'A' + 10);
		}
		else
		{
			break;
		}
	}
	return v;
}

// radix: 2, 8, 10, 16
inline int str2int(const char* p, int radix)
{
	if('0' == *p)
		radix =  ('x'==p[1] || 'X'==p[1]) ? 16 : 8;

	switch(radix)
	{
	case 2:
		return str2int_radix2(p);
	case 8:
		return str2int_radix8(p);
	case 10:
		return atoi(p);
	case 16:
		return str2int_radix16(p);
	}
	return 0;
}

#endif /* !_StrConvert_h_ */
