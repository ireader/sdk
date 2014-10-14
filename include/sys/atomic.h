#ifndef _platform_atomic_h_
#define _platform_atomic_h_

#if defined(OS_WINDOWS)
#include <Windows.h>
typedef int int32_t;
typedef __int64 int64_t;
#elif defined(OS_MAC)
#include <libkern/OSAtomic.h>
#else
typedef int int32_t;
typedef long long int64_t;
#endif

#include <assert.h>

//-------------------------------------------------------------------------------------
// int32_t atomic_increment32(volatile int32_t *value)
// int32_t atomic_decrement32(volatile int32_t *value)
// int32_t atomic_add32(volatile int32_t *value, int32_t incr)
// int atomic_cas32(volatile int32_t *value, int32_t oldvalue, int32_t newvalue)
// int atomic_cas_ptr(void* volatile *value, void *oldvalue, void *newvalue)
//-------------------------------------------------------------------------------------
// required: Windows >= Vista
// int64_t atomic_increment64(volatile int64_t *value)
// int64_t atomic_decrement64(volatile int64_t *value)
// int64_t atomic_add64(volatile int64_t *value, int32_t incr)
// int atomic_cas64(volatile int64_t *value, int64_t oldvalue, int64_t newvalue)
//-------------------------------------------------------------------------------------

#if defined(OS_WINDOWS)
inline int32_t atomic_increment32(volatile int32_t *value)
{
	assert((int32_t)value % 4 == 0);
	assert(sizeof(LONG) == sizeof(int32_t));
	return InterlockedIncrement((LONG*)value);
}

inline int32_t atomic_decrement32(volatile int32_t *value)
{
	assert((int32_t)value % 4 == 0);
	assert(sizeof(LONG) == sizeof(int32_t));
	return InterlockedDecrement((LONG*)value);
}

inline int32_t atomic_add32(volatile int32_t *value, int32_t incr)
{
	assert((int32_t)value % 4 == 0);
	assert(sizeof(LONG) == sizeof(int32_t));
	return InterlockedExchangeAdd((LONG*)value, incr) + incr; // The function returns the initial value of the Addend parameter.
}

inline int atomic_cas32(volatile int32_t *value, int32_t oldvalue, int32_t newvalue)
{
	assert((int32_t)value % 4 == 0);
	assert(sizeof(LONG) == sizeof(int32_t));
	return oldvalue == InterlockedCompareExchange((LONG*)value, newvalue, oldvalue) ? 1 : 0;
}

inline int atomic_cas_ptr(void* volatile *value, void *oldvalue, void *newvalue)
{
#if defined(_WIN64) || defined(WIN64)
	assert(0 == (int64_t)value % 8 && 0 == (int64_t)oldvalue % 8 && 0 == (int64_t)newvalue % 8);
#else
	assert(0 == (int32_t)value % 4 && 0 == (int32_t)oldvalue % 4 && 0 == (int32_t)newvalue % 4);
#endif
	return oldvalue == InterlockedCompareExchangePointer(value, newvalue, oldvalue) ? 1 : 0;
}

#if (_WIN32_WINNT >= 0x0502)

inline int64_t atomic_increment64(volatile int64_t *value)
{
	assert((int32_t)value % 8 == 0);
	assert(sizeof(LONGLONG) == sizeof(int64_t));
	return InterlockedIncrement64(value);
}

inline int64_t atomic_decrement64(volatile int64_t *value)
{
	assert((int32_t)value % 8 == 0);
	assert(sizeof(LONGLONG) == sizeof(int64_t));
	return InterlockedDecrement64(value);
}

inline int64_t atomic_add64(volatile int64_t *value, int64_t incr)
{
	assert((int32_t)value % 8 == 0);
	assert(sizeof(LONGLONG) == sizeof(int64_t));
	return InterlockedExchangeAdd64(value, incr) + incr; // The function returns the initial value of the Addend parameter.
}

inline int atomic_cas64(volatile int64_t *value, int64_t oldvalue, int64_t newvalue)
{
	assert((int32_t)value % 8 == 0);
	assert(sizeof(LONGLONG) == sizeof(int64_t));
	return oldvalue == InterlockedCompareExchange64(value, newvalue, oldvalue) ? 1 : 0;
}

#endif /* _WIN32_WINNT >= 0x0502 */

//-------------------------------------OS_MAC------------------------------------------
#elif defined(OS_MAC)
inline int32_t atomic_increment32(volatile int32_t *value)
{
	assert((int32_t)value % 4 == 0);
	return OSAtomicIncrement32(value);
}

inline int64_t atomic_increment64(volatile int64_t *value)
{
	assert((int32_t)value % 8 == 0);
	return OSAtomicIncrement64(value);
}

inline int32_t atomic_decrement32(volatile int32_t *value)
{
	assert((int32_t)value % 4 == 0);
	return OSAtomicDecrement32(value);
}

inline int64_t atomic_decrement64(volatile int64_t *value)
{
	assert((int32_t)value % 8 == 0);
	return OSAtomicDecrement64(value);
}

inline int32_t atomic_add32(volatile int32_t *value, int32_t incr)
{
	assert((int32_t)value % 4 == 0);
	return OSAtomicAdd32(incr, value);
}

inline int64_t atomic_add64(volatile int64_t *value, int64_t incr)
{
	assert((int32_t)value % 8 == 0);
	return OSAtomicAdd64(incr, value);
}

inline int atomic_cas32(volatile int32_t *value, int32_t oldvalue, int32_t newvalue)
{
	assert((int32_t)value % 4 == 0);
	return OSAtomicCompareAndSwap32(oldvalue, newvalue, value) ? 1 : 0;
}

inline int atomic_cas64(volatile int64_t *value, int64_t oldvalue, int64_t newvalue)
{
	assert((int32_t)value % 8 == 0);
	return OSAtomicCompareAndSwap64(oldvalue, newvalue, value) ? 1 : 0;
}

inline int atomic_cas_ptr(void* volatile *value, void *oldvalue, void *newvalue)
{
#if TARGET_CPU_X86_64 || TARGET_CPU_PPC64
	assert(0 == (int32_t)value % 8 && 0 == (int32_t)oldvalue % 8 && 0 == (int32_t)newvalue % 8);
#else
	assert(0 == (int32_t)value % 4 && 0 == (int32_t)oldvalue % 4 && 0 == (int32_t)newvalue % 4);
#endif
	return OSAtomicCompareAndSwapPtr(oldvalue, newvalue, value) ? 1 : 0;
}

#else

//-------------------------------------GCC----------------------------------------
#if __GNUC__>=4 && __GNUC_MINOR__>=3
inline int32_t atomic_increment32(volatile int32_t *value)
{
	assert((int32_t)value % 4 == 0);
	return __sync_add_and_fetch_4(value, 1);
}

inline int32_t atomic_decrement32(volatile int32_t *value)
{
	assert((int32_t)value % 4 == 0);
	return __sync_sub_and_fetch_4(value, 1);
}

inline int32_t atomic_add32(volatile int32_t *value, int32_t incr)
{
	assert((int32_t)value % 4 == 0);
	return __sync_add_and_fetch_4(value, incr);
}

#if __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4
inline int atomic_cas32(volatile int32_t *value, int32_t oldvalue, int32_t newvalue)
{
	assert((int32_t)value % 4 == 0);
	return __sync_bool_compare_and_swap_4(value, oldvalue, newvalue) ? 1 : 0;
}
#endif

inline int atomic_cas_ptr(void* volatile *value, void *oldvalue, void *newvalue)
{
#if defined(_WIN64) || defined(WIN64)
	assert(0 == (int32_t)value % 8 && 0 == (int32_t)oldvalue % 8 && 0 == (int32_t)newvalue % 8);
#else
	assert(0 == (int32_t)value % 4 && 0 == (int32_t)oldvalue % 4 && 0 == (int32_t)newvalue % 4);
#endif
	return __sync_bool_compare_and_swap(value, oldvalue, newvalue);
}

inline int64_t atomic_increment64(volatile int64_t *value)
{
	assert((int32_t)value % 8 == 0);
	return __sync_add_and_fetch_8(value, 1);
}

inline int64_t atomic_decrement64(volatile int64_t *value)
{
	assert((int32_t)value % 8 == 0);
	return __sync_sub_and_fetch_8(value, 1);
}

inline int64_t atomic_add64(volatile int64_t *value, int64_t incr)
{
	assert((int32_t)value % 8 == 0);
	return __sync_add_and_fetch_8(value, incr);
}

#if __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8
inline int atomic_cas64(volatile int64_t *value, int64_t oldvalue, int64_t newvalue)
{
	assert((int32_t)value % 8 == 0);
	return __sync_bool_compare_and_swap_8(value, oldvalue, newvalue) ? 1 : 0;
}
#endif

//-------------------------------------ARM----------------------------------------
#elif defined(__ARM__) || defined(__arm__)

inline int32_t atomic_add32(volatile int32_t *value, int32_t incr)
{
	assert((int32_t)value % 4 == 0);
	int a, b, c;
	asm volatile(	"0:\n\t"
					"ldr %0, [%3]\n\t"
					"add %1, %0, %4\n\t"
					"swp %2, %1, [%3]\n\t"
					"cmp %0, %2\n\t"
					"swpne %1, %2, [%3]\n\t"
					"bne 0b"
					: "=&r" (a), "=&r" (b), "=&r" (c)
					: "r" (value), "r" (incr)
					: "cc", "memory");
	return a;
}

inline int32_t atomic_increment32(volatile int32_t *value)
{
	assert((int32_t)value % 4 == 0);
	return atomic_add32(value, 1);
}

inline int32_t atomic_decrement32(volatile int32_t *value)
{
	assert((int32_t)value % 4 == 0);
	return atomic_add32(value, -1);
}

//-------------------------------------INTEL----------------------------------------
#else
#error "INTEL"
inline int32_t atomic_add32(volatile int32_t *value, int32_t incr)
{
	asm volatile ("lock; xaddl %k0,%1"
		: "=r" (incr), "=m" (*value)
		: "0" (incr), "m" (*value)
		: "memory", "cc");
	return incr;
}

inline int32_t atomic_increment32(volatile int32_t *value)
{
	assert((int32_t)value % 4 == 0);
	return atomic_add32(value, 1);
}

inline int32_t atomic_decrement32(volatile int32_t *value)
{
	assert((int32_t)value % 4 == 0);
	return atomic_add32(value, -1);
}

inline int32_t atomic_cas32(volatile int32_t *value, int32_t oldvalue, int32_t newvalue)
{
	int32_t prev;

	asm volatile ("lock; cmpxchgl %1, %2"
		: "=a" (prev)
		: "r" (newvalue), "m" (*(value)), "0"(oldvalue)
		: "memory", "cc");
	return prev;
}

inline int atomic_cas_ptr(void* volatile *value, void *oldvalue, void *newvalue)
{
	void *prev;
#if defined(X86_64)
	asm volatile ("lock; cmpxchgq %q2, %1"
		: "=a" (prev), "=m" (*value)
		: "r" ((unsigned long)newvalue), "m" (*mem), "0" ((unsigned long)oldvalue));
#else
	asm volatile ("lock; cmpxchgl %2, %1"
		: "=a" (prev), "=m" (*value)
		: "r" (newvalue), "m" (*value), "0" (oldvalue));
#endif
	return prev == oldvalue ? 1 : 0;
}
#endif

#endif /* OS_WINDOWS */

#endif /* !_platform_atomic_h_ */
