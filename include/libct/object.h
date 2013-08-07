#ifndef _libct_object_h_
#define _libct_object_h_

#include "sys/sync.h"
#include <assert.h>

namespace libct{

class object
{
protected:
	object():m_ref(1){}

public:
	virtual ~object(){ assert(0==m_ref); }

public:
	long addref()
	{
		return InterlockedIncrement(&m_ref);
	}

	long release()
	{
		long ref = InterlockedDecrement(&m_ref);
		if(0 == ref)
			delete this;
		return ref;
	}

protected:
	long get_ref_value() const{ return m_ref; }

private:
	long m_ref;
};

} // namespace

#endif /* !_libct_object_h_ */
