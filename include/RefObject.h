#ifndef _RefObject_h_
#define _RefObject_h_

#include "sys/sync.h"

class RefObject
{
protected:
	RefObject():m_ref(1){}

public:
	virtual ~RefObject(){}

public:
	long AddRef()
	{
		return InterlockedIncrement(&m_ref);
	}

	long Release()
	{
		long ref = InterlockedDecrement(&m_ref);
		if(0 == ref)
			delete this;
		return ref;
	}

protected:
	long GetRefValue() const{ return m_ref; }

private:
	long m_ref;
};

#endif /* !_RefObject_h_ */
