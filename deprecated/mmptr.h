#ifndef _mmptr_h_
#define _mmptr_h_

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <string>

class mmptr
{
public:
	mmptr(size_t n=0)
	{
		m_ptr = n ? malloc(n) : NULL;
		if(m_ptr)
			memset(m_ptr,0,n);
		m_total = m_len = m_ptr ? n : 0;
	}

	mmptr(void* p, size_t n) : m_ptr(NULL), m_len(0), m_total(0)
	{
		set(p, n);
	}

	mmptr(const char* s) : m_ptr(NULL), m_len(0), m_total(0)
	{
		set(s);
	}

	~mmptr()
	{
		_clear();
	}

	mmptr(const mmptr& obj) : m_ptr(NULL), m_len(0), m_total(0)
	{
		set(obj.m_ptr, obj.m_len);
	}

	mmptr& operator=(const mmptr& obj)
	{
		set(obj.m_ptr, obj.m_len);
		return *this;
	}

public:
	int set(const void* p, size_t n)
	{
		m_len = 0;
		return append(p, n);
	}

	int set(const char* s)
	{
		size_t n = s?strlen(s)+1:0;
		return set(s, n);
	}

	int set(const std::string& s)
	{
		return set(s.c_str(), s.length()+1);
	}

	int set(char c)
	{
		return set(&c, sizeof(c));
	}

	int append(const void* p, size_t n)
	{
		if(0 == n)
			return 0;

		if(m_total < m_len + n)
		{
			int r = reserve(m_len+n);
			if(r)
				return r;
		}

		memmove((char*)m_ptr+m_len, p, n);
		m_len += n;
		return 0;
	}

	int append(const std::string& s)
	{
		return append(s.c_str(), s.length() + 1);
	}

	int append(const char* s)
	{
		size_t n = s?strlen(s):0;
		return append(s, n);
	}

	int append(char c)
	{
		return append(&c, sizeof(c));
	}

public:
	size_t size() const
	{
		return m_len;
	}

	size_t capacity() const
	{
		return m_total;
	}

	int resize(size_t n)
	{
		void* p = realloc(m_ptr, n);
		if(!p)
			return -1;

		m_ptr = p;
		m_len = n; // change size
		m_total = n;
		return 0;
	}

	int reserve(size_t n)
	{
		if(n <= m_total)
			return 0;

		void* p = realloc(m_ptr, n);
		if(!p)
			return -1;

		m_ptr = p;
		m_total = n; // don't change size
		return 0;
	}

	void clear()
	{
		m_len = 0;
	}

public:
	void attach(void* p, size_t n)
	{
		_clear();
		m_ptr = p;
		m_total = m_len = n;
	}

	void* detach()
	{
		void* p = m_ptr;
		m_total = m_len = 0;
		m_ptr = NULL;
		return p;
	}

public:
	operator void* () { return m_ptr; }
	operator char* () { return (char*)(m_ptr?m_ptr:""); }
	operator const void* () const { return m_ptr; }
	operator const char* () const { return (char*)(m_ptr?m_ptr:""); }

	void* get() { return m_ptr; }
	const void* get() const { return m_ptr; }

public:
	mmptr& operator += (char c)
	{
		append(c);
		return *this;
	}

	mmptr& operator += (const char* s)
	{
		append(s);
		return *this;
	}

	mmptr& operator += (const std::string& s)
	{
		append(s);
		return *this;
	}

	char& operator [] (int i)
	{
		return ((char*)m_ptr)[i];
	}

	const char& operator [] (int i) const
	{
		return ((char*)m_ptr)[i];
	}

private:
	void _clear()
	{
		if(m_ptr)
		{
			assert(m_total > 0);
			free(m_ptr);
            m_ptr = NULL;
		}
		m_total = m_len = 0;
	}

private:
	void* m_ptr;
	size_t m_len;
	size_t m_total;
};

#endif /* !_mmptr_h_ */
