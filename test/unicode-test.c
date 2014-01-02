#include "unicode.h"
#include <assert.h>
#include <string.h>

void unicode_test(void)
{
	char pc[256] = {0};
	wchar_t pw[256] = {0};
	char cc[] = "1中a华人民cc共和xxxx国";
	wchar_t wc[] = L"1中a华人民cc共和xxxx国";
	unicode_to_gb18030(wc, 0, pc, sizeof(pc));
	assert(0 == strcmp(pc, cc));
	unicode_from_gb18030(pc, 0, pw, sizeof(pw));
	assert(0 == wcscmp(pw, wc));
	memset(pc, 0, sizeof(pc));
	memset(pw, 0, sizeof(pw));
	unicode_to_utf8(wc, 0, pc, sizeof(pc));
	unicode_from_utf8(pc, 0, pw, sizeof(pw));
	assert(0 == wcscmp(pw, wc));
}
