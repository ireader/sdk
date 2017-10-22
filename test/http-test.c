#include <assert.h>

void http_cookie_test(void);
void http_request_test(void);
void http_header_host_test(void);
void http_header_content_type_test(void);
void http_header_range_test(void);
void http_client_test(void);
void http_client_test2(void);

void http_test(void)
{
	http_cookie_test();
	http_request_test();
	http_header_host_test();
	http_header_content_type_test();
	http_header_range_test();

	http_client_test();
	http_client_test2();
}
