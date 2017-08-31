#ifndef _http_header_content_type_h_
#define _http_header_content_type_h_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct http_header_media_parameter
{
	const char* name;
	size_t name_len;

	const char* value;
	size_t value_len;
};

struct http_header_content_type_t
{
	char media_type[16];
	char media_subtype[16];

	size_t parameter_count;
	struct http_header_media_parameter parameters[3];
};

/// WARNING: content-type value will be changed!!!
/// usage: 
///		struct http_header_content_type_t content;
///		const char* header = "Content-Type: text/html; charset=ISO-8859-4";
///		r = http_header_content_type("text/html; charset=ISO-8859-4", &content);
///		check(r)
int http_header_content_type(const char* field, struct http_header_content_type_t* content);

#ifdef __cplusplus
}
#endif
#endif /* !_http_header_content_type_h_ */
