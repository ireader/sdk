#ifndef _platform_atomic_h_
#define _platform_atomic_h_

#if defined(OS_WINDOWS)
#include <Windows.h>
#if _MSC_VER >= 1900
#include <stdint.h>
#else
typedef int		int32_t;
typedef __int64 int64_t;
#endif
#elif defined(OS_LINUX)
#include <stdint.h>
#elif defined(OS_MAC)
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12 || __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
#if defined(__cplusplus)
#include <atomic> // for compile error
#endif
#include <stdatomic.h>
#else
#include <libkern/OSAtomic.h>
#endif
#endif

#include <stddef.h>
#include <assert.h>

//-------------------------------------------------------------------------------------
// int32_t atomic_increment32(volatile int32_t *value)
// int32_t atomic_decrement32(volatile int32_t *value)
// int32_t atomic_add32(volatile int32_t *value, int32_t incr)
// int32_t atomic_load32(volatile int32_t *value)
// int atomic_cas32(volatile int32_t *value, int32_t oldvalue, int32_t newvalue)
// int atomic_cas_ptr(void* volatile *value, void *oldvalue, void *newvalue)
//-------------------------------------------------------------------------------------
// required: Windows >= Vista
// int64_t atomic_increment64(volatile int64_t *value)
// int64_t atomic_decrement64(volatile int64_t *value)
// int64_t atomic_add64(volatile int64_t *value, int64_t incr)
// int64_t atomic_load64(volatile int64_t *value)
// int atomic_cas64(volatile int64_t *value, int64_t oldvalue, int64_t newvalue)
//-------------------------------------------------------------------------------------

#if defined(OS_WINDOWS)
static inline int32_t atomic_increment32(volatile int32_t *value)
{
	assert((intptr_t)value % 4 == 0);
	assert(sizeof(LONG) == sizeof(int32_t));
	return InterlockedIncrement((LONG volatile*)value);
}

static inline int32_t atomic_decrement32(volatile int32_t *value)
{
	assert((intptr_t)value % 4 == 0);
	assert(sizeof(LONG) == sizeof(int32_t));
	return InterlockedDecrement((LONG volatile*)value);
}

static inline int32_t atomic_add32(volatile int32_t *value, int32_t incr)
{
	assert((intptr_t)value % 4 == 0);
	assert(sizeof(LONG) == sizeof(int32_t));
	return InterlockedExchangeAdd((LONG volatile*)value, incr) + incr; // The function returns the initial value of the Addend parameter.
}

static inline int32_t atomic_load32(volatile int32_t *value)
{
    assert((intptr_t)value % 4 == 0);
    assert(sizeof(LONG) == sizeof(int32_t));
    return InterlockedOr((LONG volatile*)value, 0);
}

static inline int atomic_cas32(volatile int32_t *value, int32_t oldvalue, int32_t newvalue)
{
	assert((intptr_t)value % 4 == 0);
	assert(sizeof(LONG) == sizeof(int32_t));
	return oldvalue == InterlockedCompareExchange((LONG volatile*)value, newvalue, oldvalue) ? 1 : 0;
}

static inline int atomic_cas_ptr(void* volatile *value, void *oldvalue, void *newvalue)
{
#if defined(_WIN64) || defined(WIN64)
	assert(0 == (intptr_t)value % 8 && 0 == (intptr_t)oldvalue % 8 && 0 == (intptr_t)newvalue % 8);
#else
	assert(0 == (intptr_t)value % 4 && 0 == (intptr_t)oldvalue % 4 && 0 == (intptr_t)newvalue % 4);
#endif
	return oldvalue == InterlockedCompareExchangePointer(value, newvalue, oldvalue) ? 1 : 0;
}

#if (WINVER >= _WIN32_WINNT_WS03)

static inline int64_t atomic_increment64(volatile int64_t *value)
{
	assert((intptr_t)value % 8 == 0);
	assert(sizeof(LONGLONG) == sizeof(int64_t));
	return InterlockedIncrement64(value);
}

static inline int64_t atomic_decrement64(volatile int64_t *value)
{
	assert((intptr_t)value % 8 == 0);
	assert(sizeof(LONGLONG) == sizeof(int64_t));
	return InterlockedDecrement64(value);
}

static inline int64_t atomic_add64(volatile int64_t *value, int64_t incr)
{
	assert((intptr_t)value % 8 == 0);
	assert(sizeof(LONGLONG) == sizeof(int64_t));
	return InterlockedExchangeAdd64(value, incr) + incr; // The function returns the initial value of the Addend parameter.
}

static inline int64_t atomic_load64(volatile int64_t *value)
{
    assert((intptr_t)value % 8 == 0);
    assert(sizeof(LONGLONG) == sizeof(int64_t));
    return InterlockedOr64((LONGLONG volatile*)value, 0);
}

static inline int atomic_cas64(volatile int64_t *value, int64_t oldvalue, int64_t newvalue)
{
	assert((intptr_t)value % 8 == 0);
	assert(sizeof(LONGLONG) == sizeof(int64_t));
	return oldvalue == InterlockedCompareExchange64(value, newvalue, oldvalue) ? 1 : 0;
}

#endif /* WINVER >= 0x0502 */

//-------------------------------------OS_MAC------------------------------------------
#elif defined(OS_MAC)
static inline int32_t atomic_increment32(volatile int32_t *value)
{
	assert((intptr_t)value % 4 == 0);
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12 || __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
    return atomic_fetch_add((atomic_int_fast32_t*)value, 1) + 1;
#else
    return OSAtomicIncrement32(value);
#endif
}

static inline int64_t atomic_increment64(volatile int64_t *value)
{
	assert((intptr_t)value % 8 == 0);
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12 || __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
    return atomic_fetch_add((atomic_int_fast64_t*)value, 1) + 1;
#else
    return OSAtomicIncrement64(value);
#endif
}

static inline int32_t atomic_decrement32(volatile int32_t *value)
{
	assert((intptr_t)value % 4 == 0);
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12 || __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
    return atomic_fetch_sub((atomic_int_fast32_t*)value, 1) - 1;
#else
	return OSAtomicDecrement32(value);
#endif
}

static inline int64_t atomic_decrement64(volatile int64_t *value)
{
	assert((intptr_t)value % 8 == 0);
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12 || __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
    return atomic_fetch_sub((atomic_int_fast64_t*)value, 1) - 1;
#else
	return OSAtomicDecrement64(value);
#endif
}

static inline int32_t atomic_add32(volatile int32_t *value, int32_t incr)
{
	assert((intptr_t)value % 4 == 0);
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12 || __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
    return atomic_fetch_add((atomic_int_fast32_t*)value, incr) + incr;
#else
	return OSAtomicAdd32(incr, value);
#endif
}

static inline int64_t atomic_add64(volatile int64_t *value, int64_t incr)
{
	assert((intptr_t)value % 8 == 0);
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12 || __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
    return atomic_fetch_add((atomic_int_fast64_t*)value, incr) + incr;
#else
    return OSAtomicAdd64(incr, value);
#endif
}

static inline int32_t atomic_load32(volatile int32_t *value)
{
    assert((intptr_t)value % 4 == 0);
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12 || __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
    return atomic_load((atomic_int_fast32_t*)value);
#else
    return OSAtomicOr32(0, (uint32_t*)value);
#endif
}

static inline int64_t atomic_load64(volatile int64_t *value)
{
    assert((intptr_t)value % 8 == 0);
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12 || __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
    return atomic_load((atomic_int_fast64_t*)value);
#else
    return OSAtomicAdd64(0, value);
#endif
}

static inline int atomic_cas32(volatile int32_t *value, int32_t oldvalue, int32_t newvalue)
{
	assert((intptr_t)value % 4 == 0);
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12 || __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
    return atomic_compare_exchange_strong((atomic_int_fast32_t*)value, &oldvalue, newvalue) ? 1 : 0;
#else
	return OSAtomicCompareAndSwap32(oldvalue, newvalue, value) ? 1 : 0;
#endif
}

static inline int atomic_cas64(volatile int64_t *value, int64_t oldvalue, int64_t newvalue)
{
	assert((intptr_t)value % 8 == 0);
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12 || __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
    return atomic_compare_exchange_strong((atomic_int_fast64_t*)value, &oldvalue, newvalue) ? 1 : 0;
#else
	return OSAtomicCompareAndSwap64(oldvalue, newvalue, value) ? 1 : 0;
#endif
}

static inline int atomic_cas_ptr(void* volatile *value, void *oldvalue, void *newvalue)
{
#if TARGET_CPU_X86_64 || TARGET_CPU_PPC64
	assert(0 == (intptr_t)value % 8 && 0 == (intptr_t)oldvalue % 8 && 0 == (intptr_t)newvalue % 8);
#else
	assert(0 == (intptr_t)value % 4 && 0 == (intptr_t)oldvalue % 4 && 0 == (intptr_t)newvalue % 4);
#endif

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12 || __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
    return atomic_compare_exchange_strong((atomic_intptr_t*)value, (intptr_t*)&oldvalue, (intptr_t)newvalue) ? 1 : 0;
#else
	return OSAtomicCompareAndSwapPtr(oldvalue, newvalue, value) ? 1 : 0;
#endif
}

#else

//-------------------------------------GCC----------------------------------------
// -march=i486 32-bits
// -march=i586 64-bits
// compile with CFLAGS += -march=i586

#if __GNUC__>=4 && __GNUC_MINOR__>=1
static inline int32_t atomic_increment32(volatile int32_t *value)
{
	assert((intptr_t)value % 4 == 0);
	return __sync_add_and_fetch_4(value, 1);
}

static inline int32_t atomic_decrement32(volatile int32_t *value)
{
	assert((intptr_t)value % 4 == 0);
	return __sync_sub_and_fetch_4(value, 1);
}

static inline int32_t atomic_add32(volatile int32_t *value, int32_t incr)
{
	assert((intptr_t)value % 4 == 0);
	return __sync_add_and_fetch_4(value, incr);
}

static inline int32_t atomic_load32(volatile int32_t *value)
{
    assert((intptr_t)value % 4 == 0);
    return __sync_fetch_and_or_4(value, 0);
}

static inline int atomic_cas32(volatile int32_t *value, int32_t oldvalue, int32_t newvalue)
{
	assert((intptr_t)value % 4 == 0);
	return __sync_bool_compare_and_swap_4(value, oldvalue, newvalue) ? 1 : 0;
}

static inline int atomic_cas_ptr(void* volatile *value, void *oldvalue, void *newvalue)
{
	assert(0 == (intptr_t)value % 4 && 0 == (intptr_t)oldvalue % 4 && 0 == (intptr_t)newvalue % 4);
	return __sync_bool_compare_and_swap(value, oldvalue, newvalue) ? 1 : 0;
}

static inline int64_t atomic_increment64(volatile int64_t *value)
{
	assert((intptr_t)value % 8 == 0);
	return __sync_add_and_fetch_8(value, 1);
}

static inline int64_t atomic_decrement64(volatile int64_t *value)
{
	assert((intptr_t)value % 8 == 0);
	return __sync_sub_and_fetch_8(value, 1);
}

static inline int64_t atomic_add64(volatile int64_t *value, int64_t incr)
{
	assert((intptr_t)value % 8 == 0);
	return __sync_add_and_fetch_8(value, incr);
}

static inline int64_t atomic_load64(volatile int64_t *value)
{
    assert((intptr_t)value % 8 == 0);
    return __sync_fetch_and_or_8(value, 0);
}

static inline int atomic_cas64(volatile int64_t *value, int64_t oldvalue, int64_t newvalue)
{
	assert((intptr_t)value % 8 == 0);
	return __sync_bool_compare_and_swap_8(value, oldvalue, newvalue) ? 1 : 0;
}

//-------------------------------------ARM----------------------------------------
#elif defined(__ARM__) || defined(__arm__)

static inline int32_t atomic_add32(volatile int32_t *value, int32_t incr)
{
	assert((intptr_t)value % 4 == 0);
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

static inline int32_t atomic_increment32(volatile int32_t *value)
{
	assert((intptr_t)value % 4 == 0);
	return atomic_add32(value, 1);
}

static inline int32_t atomic_decrement32(volatile int32_t *value)
{
	assert((intptr_t)value % 4 == 0);
	return atomic_add32(value, -1);
}

//-------------------------------------INTEL----------------------------------------
#else
static inline int32_t atomic_add32(volatile int32_t *value, int32_t incr)
{
	int32_t r = incr;
	assert((intptr_t)value % 4 == 0);
	asm volatile ("lock; xaddl %0,%1"
		: "+r" (r), "+m" (*value));
	return r + incr;
}

static inline int32_t atomic_increment32(volatile int32_t *value)
{
	assert((intptr_t)value % 4 == 0);
	return atomic_add32(value, 1);
}

static inline int32_t atomic_decrement32(volatile int32_t *value)
{
	assert((intptr_t)value % 4 == 0);
	return atomic_add32(value, -1);
}

static inline int atomic_cas32(volatile int32_t *value, int32_t oldvalue, int32_t newvalue)
{
	int32_t prev = oldvalue;
	assert((intptr_t)value % 4 == 0);
	asm volatile ("lock; cmpxchgl %2, %1"
		: "+a" (prev), "+m" (*value)
		: "r" (newvalue));
	return prev == oldvalue ? 1 : 0;
}

static inline int atomic_cas64(volatile int64_t *value, int64_t oldvalue, int64_t newvalue)
{
	int64_t prev = oldvalue;
#if defined(__LP64__)
	assert((intptr_t)value % 8 == 0);
	asm volatile ("lock ; cmpxchgq %2, %1"
		: "+a" (prev), "+m" (*value)
		: "q" (newvalue));
	return prev == oldvalue ? 1 : 0;
#else
	// http://src.chromium.org/svn/trunk/src/third_party/tcmalloc/chromium/src/base/atomicops-internals-x86.h
	asm volatile ("push %%ebx\n\t"
		"movl (%3), %%ebx\n\t"    // Move 64-bit new_value into
		"movl 4(%3), %%ecx\n\t"   // ecx:ebx
		"lock; cmpxchg8b (%1)\n\t"// If edx:eax (old_value) same
		"pop %%ebx\n\t"
		: "=A" (prev)             // as contents of ptr:
		: "D" (value),              //   ecx:ebx => ptr
		"0" (oldvalue),        // else:
		"S" (&newvalue)        //   old *ptr => edx:eax
		: "memory", "%ecx");
#endif
	return prev == oldvalue ? 1 : 0;
}

static inline int64_t atomic_add64(volatile int64_t *value, int64_t incr)
{
#if defined(__LP64__)
	int64_t r = incr;
	assert((intptr_t)value % 8 == 0);
	asm volatile ("lock; xaddq %0,%1"
		: "+r" (r), "+m" (*value));
	return r + incr;
#else
	int64_t old_val, new_val;
	do {
		old_val = *value;
		new_val = old_val + incr;
	} while (atomic_cas64(value, old_val, new_val) != 1);
#endif
	return new_val;
}

static inline int64_t atomic_increment64(volatile int64_t *value)
{
	assert((intptr_t)value % 8 == 0);
	return atomic_add64(value, 1);
}

static inline int64_t atomic_decrement64(volatile int64_t *value)
{
	assert((intptr_t)value % 8 == 0);
	return atomic_add64(value, -1);
}

static inline int atomic_cas_ptr(void* volatile *value, void *oldvalue, void *newvalue)
{
	void *prev = oldvalue;
#if defined(__LP64__)
	asm volatile ("lock ; cmpxchgq %2, %1"
		: "+a" (prev), "+m" (*value)
		: "q" (newvalue));
	return prev == oldvalue ? 1 : 0;
#else
	asm volatile ("lock; cmpxchgl %2, %1"
		: "+a" (prev), "+m" (*value)
		: "r" (newvalue));
#endif
	return prev == oldvalue ? 1 : 0;
}
#endif

#endif /* OS_WINDOWS */

#endif /* !_platform_atomic_h_ */
