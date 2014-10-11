#include <stdio.h>
#include <ctype.h>

void url_test(void);
void unicode_test(void);
void utf8codec_test(void);
void thread_pool_test(void);
void systimer_test(void);
void sdp_test(void);

int main(int argc, char* argv[])
{
	//url_test();
	//unicode_test();
	//utf8codec_test();
	//thread_pool_test();
	//systimer_test();
	sdp_test();
	return 0;
}
