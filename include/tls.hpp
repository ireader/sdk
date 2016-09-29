#ifndef _tls_hpp_
#define _tls_hpp_

#include "sys/tls.h"

class ThreadLocal
{
private:
	ThreadLocal(const ThreadLocal&) {}
	ThreadLocal& operator=(const ThreadLocal&) { return *this; }

public:
	ThreadLocal() 
	{ 
		tls_create(&m_key); 
	}
	
	~ThreadLocal()
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
