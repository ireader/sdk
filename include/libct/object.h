#ifndef _libct_object_h_
#define _libct_object_h_

#include "sys/atomic.h"
#include <assert.h>

namespace libct{

class object
{
protected:
	object():m_ref(1){}

public:
	virtual ~object(){ assert(0==m_ref); }

public:
	int32_t addref()
	{
		return atomic_increment32(&m_ref);
	}

	int32_t release()
	{
		int32_t ref = atomic_decrement32(&m_ref);
		if(0 == ref)
			delete this;
		return ref;
	}

protected:
	int32_t get_ref_value() const{ return m_ref; }

private:
	int32_t m_ref;
};

struct object_deleter
{
	void operator()(object* p) {
		p->release();
	}
};

} // namespace

#endif /* !_libct_object_h_ */
