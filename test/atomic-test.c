#include "cstringext.h"
#include "ctypedef.h"
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
	*pval64 = 0x0011001100110011LL;

	assert(100 == atomic_load32(&val32));
	assert(101 == atomic_increment32(&val32) && 101==val32);
	assert(100 == atomic_decrement32(&val32) && 100==val32);
	assert(200 == atomic_add32(&val32, 100) && 200 == val32);
	assert(100 == atomic_add32(&val32, -100) && 100 == val32);
	assert(0 != atomic_cas32(&val32, 100, 101) && 101 == val32);
	assert(0 == atomic_cas32(&val32, 100, 101) && 101 == val32);

#if !defined(_WIN32_WINNT ) || _WIN32_WINNT >= 0x0502
	assert(0x0011001100110011 == atomic_load64(pval64));
	assert(0x0011001100110012 == atomic_increment64(pval64) && 0x0011001100110012==*pval64);
	assert(0x0011001100110011 == atomic_decrement64(pval64) && 0x0011001100110011==*pval64);
	assert(0x0022002200220022 == atomic_add64(pval64, 0x0011001100110011) && 0x0022002200220022 == *pval64);
	assert(0x0011001100110011 == atomic_add64(pval64, -0x0011001100110011) && 0x0011001100110011 == *pval64);
	assert(1 == atomic_cas64(pval64, 0x0011001100110011, 0x0022002200220022) && 0x0022002200220022 == *pval64);
	assert(0 == atomic_cas64(pval64, 0x0011001100110011, 0x0022002200220022) && 0x0022002200220022 == *pval64);
#endif

	assert(1 == atomic_cas_ptr((void**)&pval32, &val32, pval64) && pval64 == (int64_t*)pval32);

#if defined(OS_WINDOWS)
	_aligned_free(pval64);
#else
	free(pval64);
#endif
}
