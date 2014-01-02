#include "cstringext.h"
#include "utf8codec.h"

extern "C" void utf8codec_test(void)
{
	char cc[] = "1中a华人民cc共和xxxx国";
	wchar_t wc[] = L"1中a华人民cc共和xxxx国";

	char pc[256] = {0};
	unicode_to_utf8(wc, 0, pc, sizeof(pc));

	assert(0 == strcmp(pc, UTF8Encode(cc)));
	assert(0 == strcmp(pc, UTF8Encode(wc)));
	assert(0 == strcmp(cc, UTF8Decode(pc)));
	assert(0 == wcscmp(wc, UTF8Decode(pc)));
}
