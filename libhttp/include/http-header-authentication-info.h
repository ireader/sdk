#ifndef _http_header_authentication_info_h_
#define _http_header_authentication_info_h_

#ifdef __cplusplus
extern "C" {
#endif

struct http_header_authentication_info_t
{
	char nextnonce[128];
	char rspauth[128];
	char cnonce[128];
	int qop; // 0-auth, 1-auth-int
	int nonce_count;
};

int http_header_authentication_info(const char* field, struct http_header_authentication_info_t* content);

#ifdef __cplusplus
}
#endif

#endif /* !_http_header_authentication_info_h_ */
