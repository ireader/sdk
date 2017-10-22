#include <assert.h>

void rtsp_header_range_test(void);
void rtsp_header_session_test(void);
void rtsp_header_rtp_info_test(void);
void rtsp_header_transport_test(void);

void rtsp_test(void)
{
	rtsp_header_range_test();
	rtsp_header_session_test();
	rtsp_header_rtp_info_test();
	rtsp_header_transport_test();
}
