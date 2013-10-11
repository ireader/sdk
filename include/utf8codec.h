#ifndef _utf8_h_
#define _utf8_h_

#include "cstringext.h"
#include "unicode.h"
#include <assert.h>
#include <memory.h>
#include <string.h>

class UTF8Encode
{
public:
	// from gb18030
	UTF8Encode(const char* gb18030) : m_p(NULL)
	{
		assert(gb18030);
		int n = strlen(gb18030) + 1;
		wchar_t* wbuf = new wchar_t[n];
		n = unicode_from_gb18030(gb18030, n, wbuf, sizeof(wchar_t)*n);
		if(n > 0)
		{
			m_p = new char[(n + 1) * 4];
			memset(m_p, 0, (n + 1) * 4);
			unicode_to_utf8(wbuf, n, m_p, (n + 1) * 4);
		}
	}

	// from unicode
	UTF8Encode(const wchar_t* unicode) : m_p(NULL)
	{
		assert(unicode);
		int n = wcslen(unicode) + 1;
		m_p = new char[n * 4];
		memset(m_p, 0, n * 4);
		unicode_to_utf8(unicode, n, m_p, n * 4);
	}

	UTF8Encode(const char* text, const char* encoding) : m_p(NULL)
	{
		assert(text && encoding);
		if(0==stricmp("utf-8", encoding) || 0==stricmp("utf8", encoding))
		{
			int n = strlen(text) + 1;
			m_p = new char[n];
			strcpy(m_p, text);
		}
		else if(0==stricmp("gbk", encoding) || 0==stricmp("gb2312", encoding) || 0==stricmp("gb18030", encoding))
		{
			int n = strlen(text) + 1;
			wchar_t* wbuf = new wchar_t[n];
			n = unicode_from_gb18030(text, n, wbuf, sizeof(wchar_t)*n);
			if(n > 0)
			{
				m_p = new char[(n + 1) * 4];
				memset(m_p, 0, (n + 1) * 4);
				unicode_to_utf8(wbuf, n, m_p, (n + 1) * 4);
			}
		}
		else
		{
			assert(0);
		}
	}

	~UTF8Encode()
	{
		if(m_p)
			delete m_p;
	}

	operator const char* () const
	{
		return m_p;
	}

private:
	char* m_p;
};

class UTF8Decode
{
public:
	UTF8Decode(const char* utf8) : m_pc(NULL), m_pw(NULL)
	{
		assert(utf8);
		int n = strlen(utf8) + 1;
		m_pw = new wchar_t[n];
		memset(m_pw, 0, sizeof(wchar_t) * n);
		unicode_from_utf8(utf8, n, m_pw, sizeof(wchar_t) * n);
	}

	~UTF8Decode()
	{
		if(m_pc)
			delete m_pc;
		if(m_pw)
			delete m_pw;
	}

	const char* ToGB18030()
	{
		if(!m_pc && m_pw)
		{
			int n = wcslen(m_pw) + 1;
			m_pc = new char[n * 4];
			memset(m_pc, 0, n * 4);
			unicode_to_gb18030(m_pw, n, m_pc, n * 4);
		}
		return m_pc;
	}

	// to gb18030
	operator const char* ()
	{
		return ToGB18030();
	}

	// to unicode
	operator const wchar_t* () const
	{
		return m_pw;
	}

private:
	char* m_pc;
	wchar_t* m_pw;
};

#endif /* !_utf8_h_ */
