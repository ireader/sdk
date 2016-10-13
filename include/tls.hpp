#ifndef _tls_hpp_
#define _tls_hpp_

#include "sys/tls.h"

class ThreadLocalStorage
{
private:
	ThreadLocalStorage(const ThreadLocalStorage&) {}
	ThreadLocalStorage& operator=(const ThreadLocalStorage&) { return *this; }

public:
	ThreadLocalStorage()
	{ 
		tls_create(&m_key); 
	}
	
	~ThreadLocalStorage()
	{
		tls_destroy(m_key);
	}

	void* Get() const
	{
		return tls_getvalue(m_key);
	}

	void Set(void* value)
	{
		tls_setvalue(m_key, value);
	}

private:
	tlskey_t m_key;
};

#endif /* !_tls_hpp_ */
