#ifndef _libct_autoptr_h_
#define _libct_autoptr_h_

namespace libct
{

template<typename T>
class auto_ptr
{
	auto_ptr(const auto_ptr<T>&):m_p(0){}
	auto_ptr& operator=(const auto_ptr<T>&){ return *this; }

public:
	explicit auto_ptr(T* p=NULL)
		:m_p(p)
	{
	}

	~auto_ptr()
	{
		if(m_p)
			free(m_p);
		m_p = 0;
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
		m_p = 0;
		return p;
	}

	T* get()
	{
		return m_p;
	}

	const T* get() const
	{
		return m_p;
	}

	operator T* ()
	{
		return m_p;
	}

	T* operator -> ()
	{
		return m_p;
	}

	const T* operator -> () const
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

	T& operator [](int index)
	{
		return m_p[index];
	}

	const T& operator [](int index) const
	{
		return m_p[index];
	}

private:
	T* m_p;
};

} // namespace

#endif /* !_libct_autoptr_h_ */
