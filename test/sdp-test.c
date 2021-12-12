#include "sdp.h"
#include <stdio.h>

void sdp_a_fmtp_test(void);
void sdp_a_rtpmap_test(void);

static void sdp_test1(void)
{
	int rt;
	FILE* fp;
	void* sdp;
	char buffer[5*1024] = {0};
	fp = fopen("sdp/sdp1.txt", "r");
	rt = fread(buffer, 1, sizeof(buffer), fp);
	sdp = sdp_parse(buffer, rt);
	sdp_destroy(sdp);
	fclose(fp);
}

static void sdp_test2(void)
{
	int rt;
	FILE* fp;
	void* sdp;
	char buffer[5*1024] = {0};
	fp = fopen("sdp/sdp2.txt", "r");
	rt = fread(buffer, 1, sizeof(buffer), fp);
	sdp = sdp_parse(buffer, rt);
	sdp_destroy(sdp);
	fclose(fp);
}

void sdp_test(void)
{
	sdp_test1();
	sdp_test2();

	sdp_a_fmtp_test();
	sdp_a_rtpmap_test();
}
