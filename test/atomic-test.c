#include "cstringext.h"
#include "sys/atomic.h"
#include <assert.h>

void atomic_test(void)
{
	int32_t val32 = 100;
	int32_t *pval32 = &val32;
	int64_t *pval64;

#if defined(OS_WINDOWS)
	pval64 = _aligned_malloc(sizeof(int64_t), 8);
#else
	posix_memalign((void**)&pval64, 8, sizeof(int64_t));
#endif
	*pval64 = 0x0011001100110011L;

	assert(101 == atomic_increment32(&val32) && 101==val32);
	assert(100 == atomic_decrement32(&val32) && 100==val32);
	assert(200 == atomic_add32(&val32, 100) && 200 == val32);
	assert(100 == atomic_add32(&val32, -100) && 100 == val32);
	assert(0 != atomic_cas32(&val32, 100, 101) && 101 == val32);
	assert(0 == atomic_cas32(&val32, 100, 101) && 101 == val32);

#if defined(OS_WINDOWS ) && _WIN32_WINNT >= 0x0502
	assert(0x0011001100110012L == atomic_increment64(pval64) && 0x0011001100110012L==*pval64);
	assert(0x0011001100110011L == atomic_decrement64(pval64) && 0x0011001100110011L==*pval64);
	assert(0x0022002200220022L == atomic_add64(pval64, 0x0011001100110011L) && 0x0022002200220022L == *pval64);
	assert(0x0011001100110011L == atomic_add64(pval64, -0x0011001100110011L) && 0x0011001100110011L == *pval64);
	assert(1 == atomic_cas64(pval64, 0x0011001100110011L, 0x0022002200220022L) && 0x0022002200220022L == *pval64);
	assert(0 == atomic_cas64(pval64, 0x0011001100110011L, 0x0022002200220022L) && 0x0022002200220022L == *pval64);
#endif

	assert(1 == atomic_cas_ptr((void**)&pval32, &val32, pval64) && pval64 == (int64_t*)pval32);

#if defined(OS_WINDOWS)
	_aligned_free(pval64);
#else
	free(pval64);
#endif
}
