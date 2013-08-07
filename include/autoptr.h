#ifndef _autoptr_h_
#define _autoptr_h_

template<typename T>
class autoptr
{
	autoptr(const autoptr<T>&):m_p(NULL){}
	autoptr& operator=(const autoptr<T>&){ return *this; }

public:
	explicit autoptr(T* p)
		:m_p(p)
	{
	}

	~autoptr()
	{
		if(m_p)
			free(m_p);
		m_p = NULL;
	}

	void attach(T* p)
	{
		if(m_p)
			free(m_p);
		m_p = p;
	}

	T* detach()
	{
		T* p = m_p;
		m_p = NULL;
		return p;
	}

	operator T* ()
	{
		return m_p;
	}

	operator const T*() const
	{
		return m_p;
	}

	T** operator &()
	{
		return &m_p;
	}

	const T** operator &() const
	{
		return &m_p;
	}

private:
	T* m_p;
};

#endif /* !_autoptr_h_ */
