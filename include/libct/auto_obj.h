#ifndef _libct_auto_obj_
#define _libct_auto_obj_

namespace libct{

// typename T
// addref()/release()

template <typename T>
class auto_obj
{
public:
	explicit auto_obj(T* obj = NULL):m_obj(obj){}

	~auto_obj()
	{
		clear();
	}

	auto_obj(const auto_obj<T>& o)
	{
		m_obj = o.m_obj;
		m_obj->addref();
	}

	auto_obj& operator= (const auto_obj<T>& o)
	{
		clear();
		m_obj = o.m_obj;
		m_obj->addref();
		return *this;
	}

	void reset(T* obj)
	{
		clear();
		m_obj = obj;
	}

	T* get() const { return m_obj; }

	T* operator->() const { return m_obj; }

	operator T*&() const { return m_obj; }

	operator bool() const { return !!m_obj; }

private:
	void clear()
	{
		if (m_obj)
			m_obj->release();
		m_obj = NULL;
	}

private:
	T* m_obj;
};

} // namespace

#endif /* !_libct_auto_obj_ */
