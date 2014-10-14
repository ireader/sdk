#include "cstringext.h"
#include "sys/spinlock.h"

void spinlock_test(void)
{
	spinlock_t locker;
	assert(0 == spinlock_create(&locker));
	spinlock_lock(&locker);
	spinlock_unlock(&locker);

	if(spinlock_trylock(&locker))
		spinlock_unlock(&locker);
	assert(0 == spinlock_destroy(&locker));
}
